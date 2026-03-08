export interface SensorData {
  type: "data";
  accelX: number;
  accelY: number;
  accelZ: number;
  gyroX: number;
  gyroY: number;
  gyroZ: number;
  imuTemp: number;
  pressure: number;
  baroTemp: number;
  altitude: number;
  timestamp: number;
}

export interface WSStatusMessage {
  type: "status";
  viewers: number;
  adminConnected: boolean;
}

export interface WSAuthResult {
  type: "auth_result";
  success: boolean;
}

export interface WSAdminDisconnected {
  type: "admin_disconnected";
}

export type WSMessage =
  | SensorData
  | WSStatusMessage
  | WSAuthResult
  | WSAdminDisconnected;
