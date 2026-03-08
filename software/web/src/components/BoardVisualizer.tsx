import { useRef, useMemo, useState, Component, type ReactNode } from "react";
import { Canvas, useFrame } from "@react-three/fiber";
import { OrbitControls, useGLTF } from "@react-three/drei";
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

function enhanceMaterials(scene: THREE.Object3D) {
  scene.traverse((child) => {
    if (!(child instanceof THREE.Mesh) || !child.material) return;
    const mat = child.material as THREE.MeshStandardMaterial;
    if (!mat.color) return;

    const hex = "#" + mat.color.getHexString();
    if (hex === "#1a1a1a" || hex === "#0d0d0d" || hex === "#000000") {
      // PCB substrate — dark matte
      mat.roughness = 0.9;
      mat.metalness = 0.0;
    } else if (hex === "#c0c0c0" || hex === "#808080" || hex === "#a0a0a0" || mat.color.r > 0.6 && mat.color.g > 0.6 && mat.color.b > 0.6) {
      // Metal pads/pins — shiny
      mat.roughness = 0.2;
      mat.metalness = 0.8;
    } else if (mat.color.g > 0.3 && mat.color.r < 0.2 && mat.color.b < 0.2) {
      // Green solder mask
      mat.roughness = 0.4;
      mat.metalness = 0.1;
    } else if (mat.color.r > 0.8 && mat.color.g > 0.8 && mat.color.b < 0.3) {
      // Yellow silkscreen
      mat.roughness = 0.5;
      mat.metalness = 0.0;
      mat.emissive = new THREE.Color("#F4F244");
      mat.emissiveIntensity = 0.15;
    } else {
      // ICs/components — semi-matte plastic
      mat.roughness = 0.6;
      mat.metalness = 0.15;
    }
    mat.needsUpdate = true;
  });
}

function CyberboardModel({ accelX, accelY, accelZ }: BoardProps) {
  const { scene } = useGLTF("/Cyberboard.glb");
  const wrapperRef = useRef<THREE.Group>(null);

  const clonedScene = useMemo(() => {
    const s = scene.clone(true);
    enhanceMaterials(s);

    // Apply the intended rotation and scale, then re-center at the origin
    s.rotation.set(-Math.PI / 2, -Math.PI / 2, 0);
    s.scale.setScalar(0.1);
    s.updateMatrixWorld(true);

    const box = new THREE.Box3().setFromObject(s);
    const center = box.getCenter(new THREE.Vector3());
    s.position.sub(center);

    return s;
  }, [scene]);

  useOrientationFromGravity(wrapperRef, accelX, accelY, accelZ);

  return (
    <group ref={wrapperRef}>
      <primitive object={clonedScene} />
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
          gl.domElement.addEventListener("webglcontextlost", (e) =>
            e.preventDefault(),
          );
        }}
      >
        {/* Ambient fill */}
        <ambientLight intensity={0.8} />
        {/* Key light — bright */}
        <directionalLight position={[5, 8, 5]} intensity={1.8} />
        {/* Cyber accent from below-left */}
        <directionalLight
          position={[-4, -2, 3]}
          intensity={0.7}
          color="#F4F244"
        />
        {/* Rim light from behind */}
        <directionalLight
          position={[-2, 3, -5]}
          intensity={0.5}
          color="#4488ff"
        />
        {/* Cyber point glow */}
        <pointLight
          position={[0, 3, 0]}
          intensity={1.2}
          color="#F4F244"
          distance={12}
        />

        <SceneContent {...props} />

        <OrbitControls
          enableDamping
          dampingFactor={0.1}
          minDistance={1.5}
          maxDistance={12}
          target={[0, 0, 0]}
        />
        <gridHelper
          args={[10, 20, "#F4F244", "#1a1a1a"]}
          position={[0, -2, 0]}
        />
      </Canvas>
    </CanvasErrorBoundary>
  );
}
