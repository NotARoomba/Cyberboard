use axum::{
    extract::{
        ws::{Message, WebSocket, WebSocketUpgrade},
        State,
    },
    response::IntoResponse,
    routing::get,
    Router,
};
use futures_util::{SinkExt, StreamExt};
use serde::{Deserialize, Serialize};
use std::{
    collections::HashMap,
    env,
    net::SocketAddr,
    sync::Arc,
    time::SystemTime,
};
use tokio::sync::{mpsc, RwLock};

const FRAME_SIZE: usize = 48; // 10x f32 (40 bytes) + 1x f64 timestamp (8 bytes)

type ClientId = u64;

#[derive(Clone)]
struct ClientSender {
    tx: mpsc::UnboundedSender<Message>,
    is_admin: bool,
}

struct AppState {
    clients: HashMap<ClientId, ClientSender>,
    next_id: ClientId,
    admin_password: String,
    admin_connected: bool,
    latest_frame: Option<Vec<u8>>,
}

impl AppState {
    fn viewer_count(&self) -> usize {
        self.clients.len()
    }

    fn status_json(&self) -> String {
        serde_json::to_string(&StatusMessage {
            r#type: "status".into(),
            viewers: self.viewer_count(),
            admin_connected: self.admin_connected,
        })
        .unwrap()
    }
}

#[derive(Deserialize)]
struct AuthMessage {
    r#type: String,
    password: String,
}

#[derive(Serialize)]
struct StatusMessage {
    r#type: String,
    viewers: usize,
    admin_connected: bool,
}

fn ts() -> String {
    let now = SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap();
    let secs = now.as_secs();
    format!("{:02}:{:02}:{:02}", (secs / 3600) % 24, (secs / 60) % 60, secs % 60)
}

#[tokio::main]
async fn main() {
    dotenvy::dotenv().ok();

    let admin_password = env::var("ADMIN_PASSWORD").unwrap_or_else(|_| {
        eprintln!("[{}] WARNING: ADMIN_PASSWORD not set, defaulting to 'admin'", ts());
        "admin".into()
    });

    let port: u16 = env::var("PORT")
        .ok()
        .and_then(|p| p.parse().ok())
        .unwrap_or(3001);

    let state = Arc::new(RwLock::new(AppState {
        clients: HashMap::new(),
        next_id: 0,
        admin_password,
        admin_connected: false,
        latest_frame: None,
    }));

    // WebSocket-only server — no static file serving
    let app = Router::new()
        .route("/ws", get(ws_handler))
        .with_state(state);

    let addr = SocketAddr::from(([0, 0, 0, 0], port));
    println!("[{}] Cyberboard WS server on ws://{}/ws", ts(), addr);

    let listener = tokio::net::TcpListener::bind(addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();
}

async fn ws_handler(
    ws: WebSocketUpgrade,
    State(state): State<Arc<RwLock<AppState>>>,
) -> impl IntoResponse {
    ws.on_upgrade(move |socket| handle_socket(socket, state))
}

async fn handle_socket(socket: WebSocket, state: Arc<RwLock<AppState>>) {
    let (mut ws_sink, mut ws_stream) = socket.split();
    let (tx, mut rx) = mpsc::unbounded_channel::<Message>();

    // Register client
    let client_id = {
        let mut s = state.write().await;
        let id = s.next_id;
        s.next_id += 1;
        s.clients.insert(id, ClientSender { tx: tx.clone(), is_admin: false });
        println!("[{}] Client {} connected ({} total)", ts(), id, s.viewer_count());

        // Send latest frame to new viewer
        if let Some(ref frame) = s.latest_frame {
            let _ = tx.send(Message::Binary(frame.clone().into()));
        }

        let status = s.status_json();
        broadcast_text(&s.clients, &status, None);
        id
    };

    // Forward channel messages to WebSocket
    let send_task = tokio::spawn(async move {
        while let Some(msg) = rx.recv().await {
            if ws_sink.send(msg).await.is_err() {
                break;
            }
        }
    });

    // Process incoming messages
    while let Some(Ok(msg)) = ws_stream.next().await {
        match msg {
            Message::Text(text) => {
                handle_text(client_id, &text, &state).await;
            }
            Message::Binary(data) => {
                handle_binary(client_id, &data, &state).await;
            }
            Message::Close(_) => break,
            _ => {}
        }
    }

    // Cleanup on disconnect
    {
        let mut s = state.write().await;
        let was_admin = s.clients.get(&client_id).map_or(false, |c| c.is_admin);
        s.clients.remove(&client_id);

        if was_admin {
            s.admin_connected = false;
            println!("[{}] Admin {} disconnected", ts(), client_id);
            let msg = r#"{"type":"admin_disconnected"}"#.to_string();
            broadcast_text(&s.clients, &msg, None);
        }

        println!("[{}] Client {} disconnected ({} remaining)", ts(), client_id, s.viewer_count());
        let status = s.status_json();
        broadcast_text(&s.clients, &status, None);
    }

    send_task.abort();
}

async fn handle_text(client_id: ClientId, text: &str, state: &Arc<RwLock<AppState>>) {
    if let Ok(auth) = serde_json::from_str::<AuthMessage>(text) {
        if auth.r#type == "auth" {
            let mut s = state.write().await;
            let success = auth.password == s.admin_password;
            if success {
                if let Some(client) = s.clients.get_mut(&client_id) {
                    client.is_admin = true;
                }
                s.admin_connected = true;
                println!("[{}] Client {} auth SUCCESS", ts(), client_id);
            } else {
                println!("[{}] Client {} auth FAILED", ts(), client_id);
            }

            // Send auth result back to the client
            if let Some(client) = s.clients.get(&client_id) {
                let result = format!(r#"{{"type":"auth_result","success":{}}}"#, success);
                let _ = client.tx.send(Message::Text(result.into()));
            }

            let status = s.status_json();
            broadcast_text(&s.clients, &status, None);
        }
    }
}

async fn handle_binary(client_id: ClientId, data: &[u8], state: &Arc<RwLock<AppState>>) {
    if data.len() != FRAME_SIZE {
        return;
    }

    let mut s = state.write().await;
    if !s.clients.get(&client_id).map_or(false, |c| c.is_admin) {
        return;
    }

    s.latest_frame = Some(data.to_vec());

    // Broadcast to all except sender
    let frame = data.to_vec();
    for (&id, client) in s.clients.iter() {
        if id != client_id {
            let _ = client.tx.send(Message::Binary(frame.clone().into()));
        }
    }
}

fn broadcast_text(clients: &HashMap<ClientId, ClientSender>, text: &str, exclude: Option<ClientId>) {
    for (&id, client) in clients.iter() {
        if Some(id) != exclude {
            let _ = client.tx.send(Message::Text(text.into()));
        }
    }
}
