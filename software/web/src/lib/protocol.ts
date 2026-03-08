import type { SensorData } from "./types";

// Binary sensor frame: 48 bytes
// [10 x Float32 (40 bytes)] + [1 x Float64 timestamp (8 bytes)]
//
// Offset  Field
// ------  -----
//  0      accelX   (f32)
//  4      accelY   (f32)
//  8      accelZ   (f32)
// 12      gyroX    (f32)
// 16      gyroY    (f32)
// 20      gyroZ    (f32)
// 24      imuTemp  (f32)
// 28      pressure (f32)
// 32      baroTemp (f32)
// 36      altitude (f32)
// 40      timestamp(f64)

export const FRAME_SIZE = 48;

const encodeBuffer = new ArrayBuffer(FRAME_SIZE);
const encodeF32 = new Float32Array(encodeBuffer, 0, 10);
const encodeF64 = new Float64Array(encodeBuffer, 40, 1);

export function encodeSensorData(d: SensorData): ArrayBuffer {
  encodeF32[0] = d.accelX;
  encodeF32[1] = d.accelY;
  encodeF32[2] = d.accelZ;
  encodeF32[3] = d.gyroX;
  encodeF32[4] = d.gyroY;
  encodeF32[5] = d.gyroZ;
  encodeF32[6] = d.imuTemp;
  encodeF32[7] = d.pressure;
  encodeF32[8] = d.baroTemp;
  encodeF32[9] = d.altitude;
  encodeF64[0] = d.timestamp;
  return encodeBuffer;
}

export function decodeSensorData(buffer: ArrayBuffer): SensorData {
  const f32 = new Float32Array(buffer, 0, 10);
  const f64 = new Float64Array(buffer, 40, 1);
  return {
    type: "data",
    accelX: f32[0],
    accelY: f32[1],
    accelZ: f32[2],
    gyroX: f32[3],
    gyroY: f32[4],
    gyroZ: f32[5],
    imuTemp: f32[6],
    pressure: f32[7],
    baroTemp: f32[8],
    altitude: f32[9],
    timestamp: f64[0],
  };
}
