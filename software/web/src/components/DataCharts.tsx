import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  ResponsiveContainer,
  Tooltip,
} from "recharts";
import type { SensorData } from "@/lib/types";

interface DataChartsProps {
  history: SensorData[];
  type: "accel" | "gyro";
}

export default function DataCharts({ history, type }: DataChartsProps) {
  if (history.length === 0) {
    return (
      <div className="flex h-full items-center justify-center text-muted">
        <span className="text-xs tracking-wider">// WAITING...</span>
      </div>
    );
  }

  const step = Math.max(1, Math.floor(history.length / 100));
  const downsampled = history.filter((_, i) => i % step === 0);
  const startTime = downsampled[0].timestamp;

  const data =
    type === "accel"
      ? downsampled.map((d) => ({
          t: ((d.timestamp - startTime) / 1000).toFixed(1),
          X: parseFloat(d.accelX.toFixed(3)),
          Y: parseFloat(d.accelY.toFixed(3)),
          Z: parseFloat(d.accelZ.toFixed(3)),
        }))
      : downsampled.map((d) => ({
          t: ((d.timestamp - startTime) / 1000).toFixed(1),
          X: parseFloat(d.gyroX.toFixed(1)),
          Y: parseFloat(d.gyroY.toFixed(1)),
          Z: parseFloat(d.gyroZ.toFixed(1)),
        }));

  const gridStroke = "#1a1a1a";
  const axisStyle = { fill: "#555", fontSize: 10, fontFamily: "inherit" };
  const tooltipStyle = {
    contentStyle: {
      backgroundColor: "#0d0d0d",
      border: "1px solid rgba(244, 242, 68, 0.2)",
      borderRadius: 0,
      color: "#ededed",
      fontSize: 11,
      fontFamily: "inherit",
    },
  };

  return (
    <div className="h-full w-full p-2" style={{ minHeight: 0 }}>
      <ResponsiveContainer width="100%" height="100%">
        <LineChart data={data} margin={{ top: 4, right: 8, bottom: 4, left: -16 }}>
          <CartesianGrid strokeDasharray="3 3" stroke={gridStroke} />
          <XAxis dataKey="t" tick={axisStyle} stroke={gridStroke} />
          <YAxis tick={axisStyle} stroke={gridStroke} />
          <Tooltip {...tooltipStyle} />
          <Line
            type="monotone"
            dataKey="X"
            stroke="#ff4444"
            dot={false}
            strokeWidth={1.5}
            isAnimationActive={false}
          />
          <Line
            type="monotone"
            dataKey="Y"
            stroke="#44ff88"
            dot={false}
            strokeWidth={1.5}
            isAnimationActive={false}
          />
          <Line
            type="monotone"
            dataKey="Z"
            stroke="#4488ff"
            dot={false}
            strokeWidth={1.5}
            isAnimationActive={false}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}
