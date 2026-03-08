/**
 * Convert Cyberboard.step to Cyberboard.glb using occt-import-js
 * Merges meshes by color to minimize draw calls (988 meshes → ~10-20)
 * Run: node scripts/convert-step.mjs
 */
import { readFileSync, writeFileSync } from "fs";
import { join, dirname } from "path";
import { fileURLToPath } from "url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const ROOT = join(__dirname, "..");

function colorKey(c) {
  if (!c) return "default";
  return `${c[0]},${c[1]},${c[2]}`;
}

async function main() {
  console.log("Loading occt-import-js...");
  const occtimportjs = (await import("occt-import-js")).default;
  const occt = await occtimportjs();

  const stepPath = join(ROOT, "public", "Cyberboard.step");
  console.log(`Reading ${stepPath}...`);
  const stepBuffer = readFileSync(stepPath);
  console.log(`STEP file: ${(stepBuffer.length / 1024 / 1024).toFixed(1)} MB`);

  console.log("Parsing STEP file (this may take a minute)...");
  const result = occt.ReadStepFile(new Uint8Array(stepBuffer), null);
  console.log(`Parsed ${result.meshes.length} meshes`);

  // Group meshes by color
  const groups = new Map();
  for (const m of result.meshes) {
    const key = colorKey(m.color);
    if (!groups.has(key)) {
      groups.set(key, { color: m.color, meshes: [] });
    }
    groups.get(key).meshes.push(m);
  }
  console.log(`Grouped into ${groups.size} color groups`);

  // Merge meshes within each color group
  const merged = [];
  for (const [key, group] of groups) {
    const allPositions = [];
    const allNormals = [];
    const allIndices = [];
    let vertexOffset = 0;

    for (const m of group.meshes) {
      const positions = new Float32Array(m.attributes.position.array);
      allPositions.push(positions);

      if (m.attributes.normal) {
        allNormals.push(new Float32Array(m.attributes.normal.array));
      }

      if (m.index) {
        const indices = new Uint32Array(m.index.array);
        // Offset indices by accumulated vertex count
        const offsetIndices = new Uint32Array(indices.length);
        for (let i = 0; i < indices.length; i++) {
          offsetIndices[i] = indices[i] + vertexOffset;
        }
        allIndices.push(offsetIndices);
      }

      vertexOffset += positions.length / 3;
    }

    // Concatenate arrays
    const totalVerts = vertexOffset;
    const positions = new Float32Array(totalVerts * 3);
    let posOffset = 0;
    for (const p of allPositions) {
      positions.set(p, posOffset);
      posOffset += p.length;
    }

    let normals = null;
    if (allNormals.length === allPositions.length) {
      normals = new Float32Array(totalVerts * 3);
      let normOffset = 0;
      for (const n of allNormals) {
        normals.set(n, normOffset);
        normOffset += n.length;
      }
    }

    let indices = null;
    if (allIndices.length > 0) {
      let totalIdx = 0;
      for (const idx of allIndices) totalIdx += idx.length;
      indices = new Uint32Array(totalIdx);
      let idxOffset = 0;
      for (const idx of allIndices) {
        indices.set(idx, idxOffset);
        idxOffset += idx.length;
      }
    }

    merged.push({
      positions,
      normals,
      indices,
      vertexCount: totalVerts,
      indexCount: indices ? indices.length : 0,
      color: group.color,
    });

    console.log(`  ${key}: ${group.meshes.length} meshes → ${totalVerts} verts, ${indices ? indices.length : 0} indices`);
  }

  console.log(`Merged: ${merged.length} draw calls (was ${result.meshes.length})`);

  // Build glTF JSON + binary buffer
  const accessors = [];
  const bufferViews = [];
  const gltfMeshes = [];
  const nodes = [];
  const materials = [];
  let bufferOffset = 0;
  const binaryChunks = [];

  for (let i = 0; i < merged.length; i++) {
    const m = merged[i];
    const primitive = {};

    // Positions
    const posData = Buffer.from(m.positions.buffer, m.positions.byteOffset, m.positions.byteLength);
    const posViewIdx = bufferViews.length;
    bufferViews.push({
      buffer: 0,
      byteOffset: bufferOffset,
      byteLength: posData.length,
      target: 34962,
    });
    binaryChunks.push(posData);
    bufferOffset += posData.length;

    // Compute bounding box
    let minX = Infinity, minY = Infinity, minZ = Infinity;
    let maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;
    for (let j = 0; j < m.positions.length; j += 3) {
      const x = m.positions[j], y = m.positions[j + 1], z = m.positions[j + 2];
      if (x < minX) minX = x; if (x > maxX) maxX = x;
      if (y < minY) minY = y; if (y > maxY) maxY = y;
      if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
    }

    const posAccIdx = accessors.length;
    accessors.push({
      bufferView: posViewIdx,
      componentType: 5126,
      count: m.vertexCount,
      type: "VEC3",
      min: [minX, minY, minZ],
      max: [maxX, maxY, maxZ],
    });
    primitive.attributes = { POSITION: posAccIdx };

    // Normals
    if (m.normals) {
      const normData = Buffer.from(m.normals.buffer, m.normals.byteOffset, m.normals.byteLength);
      const normViewIdx = bufferViews.length;
      bufferViews.push({
        buffer: 0,
        byteOffset: bufferOffset,
        byteLength: normData.length,
        target: 34962,
      });
      binaryChunks.push(normData);
      bufferOffset += normData.length;

      const normAccIdx = accessors.length;
      accessors.push({
        bufferView: normViewIdx,
        componentType: 5126,
        count: m.vertexCount,
        type: "VEC3",
      });
      primitive.attributes.NORMAL = normAccIdx;
    }

    // Indices
    if (m.indices) {
      const idxData = Buffer.from(m.indices.buffer, m.indices.byteOffset, m.indices.byteLength);
      const idxViewIdx = bufferViews.length;
      bufferViews.push({
        buffer: 0,
        byteOffset: bufferOffset,
        byteLength: idxData.length,
        target: 34963,
      });
      binaryChunks.push(idxData);
      bufferOffset += idxData.length;

      const idxAccIdx = accessors.length;
      accessors.push({
        bufferView: idxViewIdx,
        componentType: 5125,
        count: m.indices.length,
        type: "SCALAR",
      });
      primitive.indices = idxAccIdx;
    }

    // Material
    const matIdx = materials.length;
    const color = m.color
      ? [m.color[0] / 255, m.color[1] / 255, m.color[2] / 255, 1.0]
      : [0.1, 0.1, 0.15, 1.0];
    materials.push({
      pbrMetallicRoughness: {
        baseColorFactor: color,
        metallicFactor: 0.1,
        roughnessFactor: 0.7,
      },
      doubleSided: true,
    });
    primitive.material = matIdx;

    gltfMeshes.push({ primitives: [primitive] });
    nodes.push({ mesh: i });
  }

  const gltfJson = {
    asset: { version: "2.0", generator: "cyberboard-convert" },
    scene: 0,
    scenes: [{ nodes: nodes.map((_, i) => i) }],
    nodes,
    meshes: gltfMeshes,
    materials,
    accessors,
    bufferViews,
    buffers: [{ byteLength: bufferOffset }],
  };

  // Encode GLB
  const jsonStr = JSON.stringify(gltfJson);
  const jsonBuf = Buffer.from(jsonStr);
  const jsonPadding = (4 - (jsonBuf.length % 4)) % 4;
  const jsonChunk = Buffer.concat([jsonBuf, Buffer.alloc(jsonPadding, 0x20)]);

  const binBuf = Buffer.concat(binaryChunks);
  const binPadding = (4 - (binBuf.length % 4)) % 4;
  const binChunk = Buffer.concat([binBuf, Buffer.alloc(binPadding, 0)]);

  const totalLength = 12 + 8 + jsonChunk.length + 8 + binChunk.length;
  const glb = Buffer.alloc(totalLength);
  let offset = 0;

  // Header
  glb.writeUInt32LE(0x46546c67, offset); offset += 4; // magic "glTF"
  glb.writeUInt32LE(2, offset); offset += 4; // version
  glb.writeUInt32LE(totalLength, offset); offset += 4; // total length

  // JSON chunk
  glb.writeUInt32LE(jsonChunk.length, offset); offset += 4;
  glb.writeUInt32LE(0x4e4f534a, offset); offset += 4; // "JSON"
  jsonChunk.copy(glb, offset); offset += jsonChunk.length;

  // Binary chunk
  glb.writeUInt32LE(binChunk.length, offset); offset += 4;
  glb.writeUInt32LE(0x004e4942, offset); offset += 4; // "BIN\0"
  binChunk.copy(glb, offset);

  const outPath = join(ROOT, "public", "Cyberboard.glb");
  writeFileSync(outPath, glb);
  console.log(
    `Wrote ${outPath} (${(glb.length / 1024 / 1024).toFixed(1)} MB)`
  );
}

main().catch(console.error);
