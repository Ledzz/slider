import React, {useCallback, useEffect, useRef, useState} from "react";

let characteristic: BluetoothRemoteGATTCharacteristic | undefined;
const serviceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914c";
const characteristicUuid = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

const log = console.log;

export function App() {
    const [payload, setPayload] = useState("");
    const [isConnected, setIsConnected] = useState(false);

    const autoConnect = useCallback(async () => {
        try {
            log("Getting existing permitted Bluetooth devices...");
            const devices = await navigator.bluetooth.getDevices();

            log("> Got " + devices.length + " Bluetooth devices.");
            for (const device of devices) {
                connectToDevice(device);
            }
        } catch (error) {
            log("Argh! " + error);
        }
    }, []);

    const readConfiguration = useCallback(async () => {
        if (!characteristic) return;

        try {
            // await characteristic.startNotifications();
            // await sendCommand("GET_CONFIG");
            //
            // characteristic.addEventListener("characteristicvaluechanged", (event) => {
            //   const value = (event.target as BluetoothRemoteGATTCharacteristic).value;
            //   if (value) {
            //     const decoder = new TextDecoder();
            //     const configString = decoder.decode(value);
            //     try {
            //       const config = JSON.parse(configString);
            //       setCurrentConfig(config);
            //       log("Received configuration:", config);
            //     } catch (e) {
            //       log("Error parsing configuration:", e);
            //     }
            //   }
            // });
        } catch (error) {
            log("Error reading configuration:", error);
        }
    }, []);

    const connectToDevice = useCallback(async (device: BluetoothDevice) => {
        const abortController = new AbortController();

        device.addEventListener(
            "advertisementreceived",
            async (event: BluetoothAdvertisingEvent) => {
                log('> Received advertisement from "' + device.name + '"...');
                abortController.abort();
                log('Connecting to GATT Server from "' + device.name + '"...');
                try {
                    const server = await device.gatt?.connect();
                    log('> Bluetooth device "' + device.name + " connected.");

                    const service = await server?.getPrimaryService(serviceUuid);
                    characteristic = await service?.getCharacteristic(characteristicUuid);
                    setIsConnected(true);

                    await readConfiguration();
                } catch (error) {
                    log("Argh! " + error);
                    setIsConnected(false);
                }
            },
            {once: true},
        );

        try {
            log('Watching advertisements from "' + device.name + '"...');
            await device.watchAdvertisements({signal: abortController.signal});
        } catch (error) {
            log("Argh! " + error);
        }
    }, [readConfiguration]);

    const manualConnect = useCallback(async () => {
        try {
            log("Requesting any Bluetooth device...");
            const device = await navigator.bluetooth.requestDevice({
                filters: [
                    {
                        services: [serviceUuid],
                    },
                ],
            });

            log("> Requested " + device.name);
            await connectToDevice(device);
        } catch (error) {
            log("Argh! " + error);
        }
    }, [connectToDevice]);

    const startedRef = useRef(false);

    useEffect(() => {
        if (!startedRef.current) {
            // Load config from memory state (would be localStorage in real app)
            autoConnect();
        }
        startedRef.current = true;
    }, [autoConnect]);

    // Save config to memory state whenever it changes
    // useEffect(() => {
    // In a real implementation with localStorage, you'd do:
    // localStorage.setItem('servoConfig', JSON.stringify(currentConfig));
    // log("Config updated:", currentConfig);
    // }, [currentConfig]);

    const sendCommand = useCallback(async (command: any) => {
        console.log("Send Command:", JSON.stringify(command));
        if (characteristic) {
            const encoder = new TextEncoder();
            await characteristic.writeValue(encoder.encode(JSON.stringify(command)));
        }
    }, []);


    const [speed, _setSpeed] = useState(0);

    const setSpeed = (s: number) => {
      _setSpeed(s);
      sendCommand({command: "speed", speed: s});
    }

    return (
        <div className="min-h-screen bg-gray-900 text-white p-6">
          <div className="max-w-6xl mx-auto">
            <h1 className="text-3xl font-bold mb-6">Servo Control Interface</h1>

            <div className="mb-6 p-4 bg-gray-800 rounded-lg">
              <h3 className="text-lg font-semibold mb-4">Speed Control</h3>
              <div className="space-y-4">
                <div className="flex items-center gap-4">
                  <label htmlFor="speed-slider" className="text-sm font-medium w-16">
                    Speed:
                  </label>
                  <div className="flex-1">
                    <input
                        id="speed-slider"
                        type="range"
                        min="0"
                        max="1000"
                        value={speed}
                        onChange={(e) => setSpeed(Number(e.target.value))}
                        className="w-full h-2 bg-gray-700 rounded-lg appearance-none cursor-pointer slider"
                    />
                  </div>
                  <div className="text-sm font-mono w-16 text-center bg-gray-700 px-2 py-1 rounded">
                    {speed}
                  </div>
                </div>
              </div>
            </div>


            {/* Connection Status */}
            <div className="mb-6 p-4 bg-gray-800 rounded-lg">
              <div className="flex items-center gap-4">
                <div className={`px-3 py-1 rounded text-sm font-medium ${
                    isConnected ? 'bg-green-600' : 'bg-red-600'
                }`}>
                  {isConnected ? 'Connected' : 'Not Connected'}
                </div>
                {!isConnected && (
                    <button
                        onClick={manualConnect}
                        className="px-4 py-2 bg-blue-600 hover:bg-blue-700 rounded transition-colors"
                    >
                      Connect via Bluetooth
                    </button>
                )}
              </div>
            </div>


            <div className="mt-8 p-4 bg-gray-800 rounded-lg">
              <h3 className="text-lg font-semibold mb-2">Debug Info</h3>
              <div className="text-sm text-gray-300">
                <p>Connection Status: {isConnected ? 'Connected' : 'Disconnected'}</p>
                <p>Device: </p>
              </div>
            </div>
          </div>
        </div>
    );
}

