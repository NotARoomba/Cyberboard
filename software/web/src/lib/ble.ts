const BLE_SERVICE_UUID = "00000000-cc7a-482a-984a-7f2ed5b3e58f";
const IMU_CHAR_UUID = "00000001-8e22-4541-9d4c-21edae82ed19";
const BARO_CHAR_UUID = "00000002-8e22-4541-9d4c-21edae82ed19";

const SEA_LEVEL_PA = 101325.0;

export interface IMUData {
  accelX: number;
  accelY: number;
  accelZ: number;
  gyroX: number;
  gyroY: number;
  gyroZ: number;
  imuTemp: number;
}

export interface BaroData {
  pressure: number;
  baroTemp: number;
  altitude: number;
}

export interface BLECallbacks {
  onIMUData: (data: IMUData) => void;
  onBaroData: (data: BaroData) => void;
  onDisconnect: () => void;
}

let device: BluetoothDevice | null = null;

export function isBLESupported(): boolean {
  return typeof navigator !== "undefined" && "bluetooth" in navigator;
}

export async function connectBLE(callbacks: BLECallbacks): Promise<string> {
  if (!isBLESupported()) {
    throw new Error("Web Bluetooth is not supported. Use Chrome/Edge on HTTPS or localhost.");
  }

  device = await navigator.bluetooth.requestDevice({
    filters: [{ name: "Cyberboard" }, { namePrefix: "Cyber" }],
    optionalServices: [BLE_SERVICE_UUID],
  });

  device.addEventListener("gattserverdisconnected", () => {
    callbacks.onDisconnect();
  });

  const server = await device.gatt!.connect();
  const service = await server.getPrimaryService(BLE_SERVICE_UUID);

  // Subscribe to IMU characteristic
  try {
    const imuChar = await service.getCharacteristic(IMU_CHAR_UUID);
    await imuChar.startNotifications();
    imuChar.addEventListener("characteristicvaluechanged", (event) => {
      const value = (event.target as BluetoothRemoteGATTCharacteristic).value!;
      if (value.byteLength >= 28) {
        callbacks.onIMUData({
          accelX: value.getFloat32(0, true),
          accelY: value.getFloat32(4, true),
          accelZ: value.getFloat32(8, true),
          gyroX: value.getFloat32(12, true),
          gyroY: value.getFloat32(16, true),
          gyroZ: value.getFloat32(20, true),
          imuTemp: value.getFloat32(24, true),
        });
      }
    });
  } catch (e) {
    console.warn("IMU characteristic not found:", e);
  }

  // Subscribe to Baro characteristic
  try {
    const baroChar = await service.getCharacteristic(BARO_CHAR_UUID);
    await baroChar.startNotifications();
    baroChar.addEventListener("characteristicvaluechanged", (event) => {
      const value = (event.target as BluetoothRemoteGATTCharacteristic).value!;
      if (value.byteLength >= 8) {
        const pressure = value.getFloat32(0, true);
        const baroTemp = value.getFloat32(4, true);
        const altitude =
          pressure > 0
            ? 44330.0 * (1.0 - Math.pow(pressure / SEA_LEVEL_PA, 1.0 / 5.255))
            : 0;
        callbacks.onBaroData({ pressure, baroTemp, altitude });
      }
    });
  } catch (e) {
    console.warn("Baro characteristic not found:", e);
  }

  return device.name || "Cyberboard";
}

export function disconnectBLE(): void {
  if (device?.gatt?.connected) {
    device.gatt.disconnect();
  }
  device = null;
}
