interface SensorValue {
  label: string;
  value: number;
  color: string;
  unit?: string;
  decimals?: number;
}

interface SensorCardProps {
  title: string;
  unit?: string;
  values: SensorValue[];
}

export default function SensorCard({ title, unit, values }: SensorCardProps) {
  return (
    <div className="glow-border rounded border border-border-cyber bg-card">
      <div className="border-b border-border px-4 py-2">
        <span className="text-sm tracking-wider text-cyber">// {title}</span>
      </div>
      <div className="space-y-2 px-4 py-3">
        {values.map((v) => {
          const decimals = v.decimals ?? 2;
          const displayUnit = v.unit || unit || "";

          return (
            <div
              key={v.label}
              className="flex items-center justify-between text-sm"
            >
              <div className="flex items-center gap-2">
                <div
                  className="h-1.5 w-1.5 rounded-full"
                  style={{ backgroundColor: v.color }}
                />
                <span className="text-text-dim">{v.label}</span>
              </div>
              <span className="font-display tabular-nums" style={{ color: v.color }}>
                {v.value >= 0 ? "+" : ""}
                {v.value.toFixed(decimals)}{" "}
                <span className="text-xs text-muted">{displayUnit}</span>
              </span>
            </div>
          );
        })}
      </div>
    </div>
  );
}
