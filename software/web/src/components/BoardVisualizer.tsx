import { useRef, useMemo, useState, Component, type ReactNode } from "react";
import { Canvas, useFrame } from "@react-three/fiber";
import { OrbitControls, useGLTF, Center } from "@react-three/drei";
import * as THREE from "three";

interface BoardProps {
  accelX: number;
  accelY: number;
  accelZ: number;
}

class CanvasErrorBoundary extends Component<
  { children: ReactNode; fallback: ReactNode },
  { hasError: boolean }
> {
  constructor(props: { children: ReactNode; fallback: ReactNode }) {
    super(props);
    this.state = { hasError: false };
  }
  static getDerivedStateFromError() {
    return { hasError: true };
  }
  render() {
    if (this.state.hasError) return this.props.fallback;
    return this.props.children;
  }
}

function useOrientationFromGravity(
  ref: React.RefObject<THREE.Group | null>,
  accelX: number,
  accelY: number,
  accelZ: number,
) {
  const targetQuat = useRef(new THREE.Quaternion());
  useFrame(() => {
    if (!ref.current) return;
    // Remap IMU axes (Z-up) to Three.js (Y-up): X→X, Y→Z, Z→Y
    const gravity = new THREE.Vector3(accelZ, -accelY, accelX);
    if (gravity.length() > 0.01) {
      gravity.normalize();
      targetQuat.current.setFromUnitVectors(
        new THREE.Vector3(0, 1, 0),
        gravity,
      );
    }
    ref.current.quaternion.slerp(targetQuat.current, 0.08);
  });
}

function CyberboardModel({ accelX, accelY, accelZ }: BoardProps) {
  const { scene } = useGLTF("/Cyberboard.glb");
  const wrapperRef = useRef<THREE.Group>(null);

  // Clone scene so React strict mode re-renders don't dispose the cached original
  const clonedScene = useMemo(() => scene.clone(true), [scene]);

  useOrientationFromGravity(wrapperRef, accelX, accelY, accelZ);

  return (
    <group ref={wrapperRef}>
      <Center>
        <primitive
          object={clonedScene}
          scale={0.2}
          rotation={[-Math.PI / 2, -Math.PI / 2, 0]}
        />
      </Center>
    </group>
  );
}

function FallbackBoard({ accelX, accelY, accelZ }: BoardProps) {
  const groupRef = useRef<THREE.Group>(null);
  useOrientationFromGravity(groupRef, accelX, accelY, accelZ);

  return (
    <group ref={groupRef}>
      <mesh>
        <boxGeometry args={[3.2, 2.2, 0.08]} />
        <meshStandardMaterial color="#1a1a1a" roughness={0.8} metalness={0.1} />
      </mesh>
      <mesh position={[0, 0, 0.05]}>
        <boxGeometry args={[2.8, 1.8, 0.01]} />
        <meshStandardMaterial
          color="#F4F244"
          roughness={0.5}
          transparent
          opacity={0.15}
        />
      </mesh>
      <mesh position={[0, 0, 0.08]}>
        <boxGeometry args={[0.5, 0.5, 0.08]} />
        <meshStandardMaterial color="#0d0d0d" roughness={0.4} metalness={0.6} />
      </mesh>
    </group>
  );
}

// Error catcher inside Canvas (class component for componentDidCatch)
class ModelErrorCatcher extends Component<
  { children: ReactNode; onError: () => void },
  { hasError: boolean }
> {
  constructor(props: { children: ReactNode; onError: () => void }) {
    super(props);
    this.state = { hasError: false };
  }
  static getDerivedStateFromError() {
    return { hasError: true };
  }
  componentDidCatch() {
    this.props.onError();
  }
  render() {
    if (this.state.hasError) return null;
    return this.props.children;
  }
}

function SceneContent(props: BoardProps) {
  const [glbFailed, setGlbFailed] = useState(false);

  if (glbFailed) {
    return <FallbackBoard {...props} />;
  }

  return (
    <ModelErrorCatcher onError={() => setGlbFailed(true)}>
      <CyberboardModel {...props} />
    </ModelErrorCatcher>
  );
}

export default function BoardVisualizer(props: BoardProps) {
  return (
    <CanvasErrorBoundary
      fallback={
        <div className="flex h-full items-center justify-center">
          <span className="text-sm text-muted tracking-wider">
            // 3D UNAVAILABLE
          </span>
        </div>
      }
    >
      <Canvas
        camera={{ position: [3, 2.5, 3], fov: 40 }}
        style={{ background: "transparent" }}
        gl={{ antialias: true, alpha: true, powerPreference: "default" }}
        onCreated={({ gl }) => {
          const canvas = gl.domElement;
          canvas.addEventListener("webglcontextlost", (e) => {
            e.preventDefault();
          });
          canvas.addEventListener("webglcontextrestored", () => {
            gl.compile(gl.domElement as unknown as THREE.Object3D, gl.domElement as unknown as THREE.Camera);
          });
        }}
      >
        {/* Ambient fill */}
        <ambientLight intensity={0.6} />
        {/* Key light */}
        <directionalLight position={[5, 8, 5]} intensity={1.2} />
        {/* Cyber accent from below-left */}
        <directionalLight
          position={[-4, -2, 3]}
          intensity={0.5}
          color="#F4F244"
        />
        {/* Rim light from behind */}
        <directionalLight
          position={[-2, 3, -5]}
          intensity={0.4}
          color="#4488ff"
        />
        {/* Cyber point glow */}
        <pointLight
          position={[0, 2, 0]}
          intensity={0.8}
          color="#F4F244"
          distance={10}
        />
        {/* Fill from below */}
        <pointLight
          position={[0, -2, 0]}
          intensity={0.3}
          color="#44ff88"
          distance={8}
        />

        <SceneContent {...props} />

        <OrbitControls
          enableDamping
          dampingFactor={0.1}
          minDistance={1.5}
          maxDistance={12}
        />
        <gridHelper
          args={[10, 20, "#F4F244", "#1a1a1a"]}
          position={[0, -2, 0]}
        />
      </Canvas>
    </CanvasErrorBoundary>
  );
}
