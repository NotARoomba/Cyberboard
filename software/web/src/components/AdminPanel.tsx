import { useState, useRef, useCallback } from "react";
import { connectBLE, disconnectBLE, isBLESupported } from "@/lib/ble";
import type { SensorData } from "@/lib/types";

interface AdminPanelProps {
  isAdmin: boolean;
  bleConnected: boolean;
  onAuth: (password: string) => void;
  onData: (data: Partial<SensorData>) => void;
  setBleConnected: (connected: boolean) => void;
}

export default function AdminPanel({
  isAdmin,
  bleConnected,
  onAuth,
  onData,
  setBleConnected,
}: AdminPanelProps) {
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const [deviceName, setDeviceName] = useState("");
  const [bleSupported] = useState(() =>
    typeof window !== "undefined" ? isBLESupported() : false
  );
  const latestIMU = useRef({
    accelX: 0,
    accelY: 0,
    accelZ: 1,
    gyroX: 0,
    gyroY: 0,
    gyroZ: 0,
    imuTemp: 0,
  });
  const latestBaro = useRef({ pressure: 0, baroTemp: 0, altitude: 0 });

  const handleAuth = () => {
    if (!password.trim()) return;
    setError("");
    onAuth(password);
  };

  const handleBLEConnect = useCallback(async () => {
    setError("");
    try {
      const name = await connectBLE({
        onIMUData: (imu) => {
          latestIMU.current = imu;
          onData({
            ...imu,
            ...latestBaro.current,
          });
        },
        onBaroData: (baro) => {
          latestBaro.current = baro;
          onData({
            ...latestIMU.current,
            ...baro,
          });
        },
        onDisconnect: () => {
          setBleConnected(false);
          setDeviceName("");
        },
      });
      setBleConnected(true);
      setDeviceName(name);
    } catch (e) {
      setError(e instanceof Error ? e.message : "BLE connection failed");
    }
  }, [onData, setBleConnected]);

  const handleBLEDisconnect = () => {
    disconnectBLE();
    setBleConnected(false);
    setDeviceName("");
  };

  return (
    <div className="flex flex-col items-center gap-2">
      {!isAdmin ? (
        <>
          <div className="flex items-center gap-1">
            <input
              type="password"
              placeholder="Password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              onKeyDown={(e) => e.key === "Enter" && handleAuth()}
              className="w-28 px-2 py-0.5 text-xs"
            />
            <button
              onClick={handleAuth}
              className="border border-cyber bg-cyber/10 px-2 py-0.5 text-xs text-cyber transition-all hover:bg-cyber/20"
            >
              AUTH
            </button>
          </div>
          {error && <span className="text-[10px] text-danger">{error}</span>}
        </>
      ) : (
        <>
          {!bleSupported ? (
            <span className="text-[10px] text-danger">
              No Web Bluetooth. Use Chrome/Edge.
            </span>
          ) : bleConnected ? (
            <div className="flex items-center gap-2">
              <div className="flex items-center gap-1">
                <div className="h-1.5 w-1.5 bg-cyber pulse-cyber" />
                <span className="text-xs text-cyber">
                  {deviceName || "BLE"}
                </span>
              </div>
              <button
                onClick={handleBLEDisconnect}
                className="border border-danger/50 px-2 py-0.5 text-xs text-danger transition-all hover:bg-danger/10"
              >
                DISCONNECT
              </button>
            </div>
          ) : (
            <button
              onClick={handleBLEConnect}
              className="border border-cyber px-3 py-0.5 text-xs text-cyber transition-all hover:bg-cyber/20"
            >
              CONNECT BLE
            </button>
          )}
          {error && <span className="text-[10px] text-danger">{error}</span>}
        </>
      )}
    </div>
  );
}
