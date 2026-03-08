import { useState, useEffect, useRef, useCallback, lazy, Suspense } from "react";
import AdminPanel from "./AdminPanel";
import DataCharts from "./DataCharts";
import type { SensorData, WSMessage } from "@/lib/types";
import {
  encodeSensorData,
  decodeSensorData,
  FRAME_SIZE,
} from "@/lib/protocol";

const BoardVisualizer = lazy(() => import("./BoardVisualizer"));

const MAX_HISTORY = 60;
const SEA_LEVEL_PA = 101325.0;

function generateDemoData(t: number): SensorData {
  const s = t / 1000;
  return {
    type: "data",
    accelX: Math.sin(s * 0.7) * 0.3,
    accelY: Math.cos(s * 0.5) * 0.2,
    accelZ: 1.0 + Math.sin(s * 0.3) * 0.05,
    gyroX: Math.sin(s * 1.2) * 15,
    gyroY: Math.cos(s * 0.8) * 10,
    gyroZ: Math.sin(s * 0.4) * 5,
    imuTemp: 25.0 + Math.sin(s * 0.1) * 2,
    pressure: 101325 + Math.sin(s * 0.05) * 200,
    baroTemp: 24.5 + Math.sin(s * 0.1) * 1.5,
    altitude: Math.sin(s * 0.05) * 1.7,
    timestamp: t,
  };
}

function Bento({
  children,
  className = "",
}: {
  children: React.ReactNode;
  className?: string;
}) {
  return (
    <div
      className={`border border-border-cyber bg-card ${className}`}
    >
      {children}
    </div>
  );
}

function Metric({
  label,
  value,
  unit,
  decimals = 2,
  color = "#F4F244",
  size = "md",
}: {
  label: string;
  value: number;
  unit: string;
  decimals?: number;
  color?: string;
  size?: "sm" | "md" | "lg";
}) {
  const textSize =
    size === "lg"
      ? "text-4xl xl:text-5xl"
      : size === "md"
        ? "text-3xl xl:text-4xl"
        : "text-xl xl:text-2xl";
  return (
    <div className="flex flex-col items-center justify-center gap-0.5">
      <span className="text-[10px] tracking-widest text-muted uppercase">
        {label}
      </span>
      <span
        className={`font-display tabular-nums leading-none ${textSize}`}
        style={{ color }}
      >
        {value.toFixed(decimals)}
      </span>
      <span className="text-[10px] text-muted">{unit}</span>
    </div>
  );
}

function AxisValue({
  axis,
  value,
  color,
  unit,
}: {
  axis: string;
  value: number;
  color: string;
  unit: string;
}) {
  return (
    <div className="flex items-center gap-1.5">
      <div className="h-2 w-2" style={{ backgroundColor: color }} />
      <span className="text-xs text-muted">{axis}</span>
      <span
        className="font-display text-xl tabular-nums xl:text-2xl"
        style={{ color }}
      >
        {value.toFixed(2)}
      </span>
      <span className="text-[10px] text-muted">{unit}</span>
    </div>
  );
}

export default function Dashboard() {
  const [isAdmin, setIsAdmin] = useState(false);
  const [bleConnected, setBleConnected] = useState(false);
  const [wsConnected, setWsConnected] = useState(false);
  const [viewers, setViewers] = useState(0);
  const [adminOnline, setAdminOnline] = useState(false);
  const [currentData, setCurrentData] = useState<SensorData | null>(null);
  const [history, setHistory] = useState<SensorData[]>([]);
  const [showAdmin, setShowAdmin] = useState(false);
  const [demoMode, setDemoMode] = useState(false);
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectRef = useRef<ReturnType<typeof setTimeout>>(undefined);
  const demoRef = useRef<ReturnType<typeof setInterval>>(undefined);
  const savedPasswordRef = useRef<string | null>(null);

  useEffect(() => {
    if (demoMode) {
      const startTime = Date.now();
      demoRef.current = setInterval(() => {
        const data = generateDemoData(Date.now() - startTime);
        setCurrentData(data);
        setHistory((prev) => {
          const next = [...prev, data];
          return next.length > MAX_HISTORY ? next.slice(-MAX_HISTORY) : next;
        });
      }, 50);
    } else {
      clearInterval(demoRef.current);
    }
    return () => clearInterval(demoRef.current);
  }, [demoMode]);

  const handleSensorData = useCallback((data: SensorData) => {
    setCurrentData(data);
    setHistory((prev) => {
      const next = [...prev, data];
      return next.length > MAX_HISTORY ? next.slice(-MAX_HISTORY) : next;
    });
  }, []);

  const connectWebSocket = useCallback(() => {
    const ws = new WebSocket("wss://api.cyberboard.notaroomba.dev/ws");
    ws.binaryType = "arraybuffer";
    wsRef.current = ws;

    ws.onopen = () => {
      setWsConnected(true);
      if (savedPasswordRef.current) {
        ws.send(
          JSON.stringify({ type: "auth", password: savedPasswordRef.current })
        );
      }
    };
    ws.onclose = () => {
      setWsConnected(false);
      reconnectRef.current = setTimeout(connectWebSocket, 2000);
    };
    ws.onerror = () => ws.close();

    ws.onmessage = (event) => {
      if (event.data instanceof ArrayBuffer) {
        if (event.data.byteLength === FRAME_SIZE) {
          handleSensorData(decodeSensorData(event.data));
        }
        return;
      }
      try {
        const msg: WSMessage = JSON.parse(event.data);
        if (msg.type === "status") {
          setViewers(msg.viewers);
          setAdminOnline(msg.adminConnected);
        } else if (msg.type === "auth_result") {
          setIsAdmin(msg.success);
          if (!msg.success) savedPasswordRef.current = null;
        } else if (msg.type === "admin_disconnected") {
          setAdminOnline(false);
        }
      } catch {
        /* ignore */
      }
    };
  }, [handleSensorData]);

  useEffect(() => {
    connectWebSocket();
    return () => {
      clearTimeout(reconnectRef.current);
      wsRef.current?.close();
    };
  }, [connectWebSocket]);

  const authenticate = useCallback((password: string) => {
    savedPasswordRef.current = password;
    wsRef.current?.send(JSON.stringify({ type: "auth", password }));
  }, []);

  const sendData = useCallback(
    (data: Partial<SensorData>) => {
      const fullData: SensorData = {
        type: "data",
        accelX: 0,
        accelY: 0,
        accelZ: 1,
        gyroX: 0,
        gyroY: 0,
        gyroZ: 0,
        imuTemp: 0,
        pressure: 0,
        baroTemp: 0,
        altitude: 0,
        timestamp: Date.now(),
        ...data,
      };
      handleSensorData(fullData);
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        wsRef.current.send(encodeSensorData(fullData));
      }
    },
    [handleSensorData]
  );

  const d = currentData;
  const altitude = d
    ? d.altitude ||
      (d.pressure > 0
        ? 44330.0 * (1.0 - Math.pow(d.pressure / SEA_LEVEL_PA, 1.0 / 5.255))
        : 0)
    : 0;

  const isLive = adminOnline || bleConnected || demoMode;

  return (
    <div className="cyber-grid h-screen w-full overflow-hidden p-2 xl:p-3">
      {/* 3x3 Bento Grid — fits 16:9 viewport, wraps on mobile */}
      <div className="grid h-full grid-cols-1 gap-2 md:grid-cols-3 md:grid-rows-3 xl:gap-3">

        {/* ═══ ROW 1 ═══ */}

        {/* (1,1) Acceleration Values */}
        <Bento className="flex flex-col items-center justify-center gap-2 p-4">
          <span className="text-[10px] tracking-widest text-cyber uppercase">
            // Acceleration
          </span>
          <div className="flex flex-col gap-2">
            <AxisValue axis="X" value={d?.accelX ?? 0} color="#ff4444" unit="g" />
            <AxisValue axis="Y" value={d?.accelY ?? 0} color="#44ff88" unit="g" />
            <AxisValue axis="Z" value={d?.accelZ ?? 0} color="#4488ff" unit="g" />
          </div>
        </Bento>

        {/* (1,2) 3D Model */}
        <Bento className="relative overflow-hidden">
          <Suspense
            fallback={
              <div className="flex h-full items-center justify-center text-muted">
                Loading 3D...
              </div>
            }
          >
            <BoardVisualizer
              accelX={d?.accelX ?? 0}
              accelY={d?.accelY ?? 0}
              accelZ={d?.accelZ ?? 1}
            />
          </Suspense>
        </Bento>

        {/* (1,3) Gyroscope Values */}
        <Bento className="flex flex-col items-center justify-center gap-2 p-4">
          <span className="text-[10px] tracking-widest text-cyber uppercase">
            // Gyroscope
          </span>
          <div className="flex flex-col gap-2">
            <AxisValue axis="X" value={d?.gyroX ?? 0} color="#ff4444" unit="°/s" />
            <AxisValue axis="Y" value={d?.gyroY ?? 0} color="#44ff88" unit="°/s" />
            <AxisValue axis="Z" value={d?.gyroZ ?? 0} color="#4488ff" unit="°/s" />
          </div>
        </Bento>

        {/* ═══ ROW 2 ═══ */}

        {/* (2,1) Acceleration Chart */}
        <Bento className="flex flex-col overflow-hidden">
          <div className="border-b border-border px-3 py-1">
            <span className="text-[10px] tracking-wider text-cyber">// ACCEL</span>
          </div>
          <div className="flex-1" style={{ minHeight: 0 }}>
            <DataCharts history={history} type="accel" />
          </div>
        </Bento>

        {/* (2,2) ★ CYBERBOARD — CENTER ★ */}
        <Bento className="flex flex-col items-center justify-center p-4">
          <h1 className="font-title glow-text-strong flicker text-3xl tracking-widest text-cyber xl:text-5xl">
            CYBERBOARD
          </h1>

          <div className="mt-2 flex flex-col items-center gap-1">
            <div
              className={`text-lg font-display tracking-wider ${isLive ? "text-cyber glow-text" : "text-danger"}`}
            >
              {isLive
                ? demoMode
                  ? "DEMO"
                  : "CONNECTED"
                : "NO DATA"}
            </div>
            <div className="flex items-center gap-1.5">
              <div
                className={`h-1.5 w-1.5 ${wsConnected ? "bg-success pulse-cyber" : "bg-danger"}`}
              />
              <span className="text-[10px] text-muted">
                {viewers} viewer{viewers !== 1 ? "s" : ""}
              </span>
            </div>
          </div>

          {/* Buttons */}
          <div className="mt-2 flex gap-2">
            <button
              onClick={() => setDemoMode(!demoMode)}
              className={`border px-3 py-0.5 text-[10px] tracking-wider transition-all ${
                demoMode
                  ? "border-cyber bg-cyber/10 text-cyber"
                  : "border-border text-muted hover:border-border-cyber hover:text-foreground"
              }`}
            >
              DEMO
            </button>
            <button
              onClick={() => setShowAdmin(!showAdmin)}
              className={`border px-3 py-0.5 text-[10px] tracking-wider transition-all ${
                showAdmin
                  ? "border-cyber bg-cyber/10 text-cyber"
                  : "border-border text-muted hover:border-border-cyber hover:text-foreground"
              }`}
            >
              {isAdmin ? "ADMIN" : "LOGIN"}
            </button>
          </div>

          {/* Inline auth/BLE panel */}
          {showAdmin && (
            <div className="mt-3 w-full border-t border-border pt-3">
              <AdminPanel
                isAdmin={isAdmin}
                bleConnected={bleConnected}
                onAuth={authenticate}
                onData={sendData}
                setBleConnected={setBleConnected}
              />
            </div>
          )}
        </Bento>

        {/* (2,3) Gyroscope Chart */}
        <Bento className="flex flex-col overflow-hidden">
          <div className="border-b border-border px-3 py-1">
            <span className="text-[10px] tracking-wider text-cyber">// GYRO</span>
          </div>
          <div className="flex-1" style={{ minHeight: 0 }}>
            <DataCharts history={history} type="gyro" />
          </div>
        </Bento>

        {/* ═══ ROW 3 ═══ */}

        {/* (3,1) Temperature */}
        <Bento className="flex items-center justify-center p-3">
          <Metric
            label="Temperature"
            value={d?.imuTemp ?? 0}
            unit="°C"
            decimals={1}
            size="lg"
          />
        </Bento>

        {/* (3,2) Altitude */}
        <Bento className="flex items-center justify-center p-3">
          <Metric
            label="Altitude"
            value={altitude}
            unit="m"
            decimals={1}
            size="lg"
            color="#44ff88"
          />
        </Bento>

        {/* (3,3) Pressure */}
        <Bento className="flex items-center justify-center p-3">
          <Metric
            label="Pressure"
            value={(d?.pressure ?? 0) / 1000}
            unit="kPa"
            decimals={2}
            size="lg"
          />
        </Bento>
      </div>
    </div>
  );
}
