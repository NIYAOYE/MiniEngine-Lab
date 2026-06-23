import { CROP_DB } from '@/lib/toolClient';

/**
 * SVG crop sprite — the visual signature of the editor.
 *
 * It renders an actual little plant whose silhouette reads its species and its
 * growth stage: a tight seed mound early, a fan of leaves mid-growth, and a
 * distinct fruit when mature (a pale root crown for parsnip, a creamy curd head
 * for cauliflower). Everything is derived from `stage`, so the field visibly
 * tells a growth story rather than just colouring squares.
 */
interface Props {
  cropId: string;
  stage: number;
  size?: number;
}

/** A single upright leaf, rotated `angle°` about the base point (40,72). */
function Leaf({ angle, len, color, dark }: { angle: number; len: number; color: string; dark: string }) {
  return (
    <g transform={`translate(40 72) rotate(${angle})`}>
      <path
        d={`M0,0 C -6,${-len * 0.5} -5,${-len * 0.9} 0,${-len} C 5,${-len * 0.9} 6,${-len * 0.5} 0,0 Z`}
        fill={color}
        stroke={dark}
        strokeWidth={0.8}
      />
      <line x1="0" y1="-2" x2="0" y2={-len * 0.85} stroke={dark} strokeWidth="0.7" opacity="0.5" />
    </g>
  );
}

export function CropSprite({ cropId, stage, size = 58 }: Props) {
  const stages = CROP_DB[cropId] ?? ['seed'];
  const maxStage = stages.length - 1;
  const t = maxStage > 0 ? stage / maxStage : 0; // 0..1 growth
  const isParsnip = cropId === 'parsnip';

  // Leaf colour deepens with growth.
  const leaf = isParsnip ? '#5fae4a' : '#6fae57';
  const leafDark = isParsnip ? '#3c7a32' : '#46813a';

  // Leaf count + length scale with stage.
  const leafCount = Math.min(2 + stage, 7);
  const baseLen = 12 + t * 30;

  // Stage 0 = seed: just a mound + seed, no leaves yet.
  if (stage === 0) {
    return (
      <svg viewBox="0 0 80 80" width={size} height={size} aria-hidden>
        <ellipse cx="40" cy="70" rx="13" ry="5" fill="#000" opacity="0.18" />
        <ellipse cx="40" cy="66" rx="8" ry="5.5" fill="#4a3829" />
        <circle cx="40" cy="64" r="2.4" fill={isParsnip ? '#caa15a' : '#e6e0c8'} stroke={leafDark} strokeWidth="0.6" />
        <path d="M40,62 q1,-3 3,-4" stroke={leaf} strokeWidth="1.4" fill="none" strokeLinecap="round" />
      </svg>
    );
  }

  const leaves = Array.from({ length: leafCount }, (_, i) => {
    const spread = 64; // total fan degrees
    const angle = -spread / 2 + (spread / Math.max(1, leafCount - 1)) * i;
    const lenJitter = i % 2 === 0 ? 1 : 0.86;
    return { angle, len: baseLen * lenJitter };
  });

  // Fruit for the heading/mature stages.
  const showHead = !isParsnip && stage >= maxStage - 1; // cauliflower: heading + mature
  const headR = showHead ? 6 + (stage - (maxStage - 1)) * 4 : 0;
  const showRoot = isParsnip && stage >= maxStage; // parsnip: mature crown

  return (
    <svg viewBox="0 0 80 80" width={size} height={size} aria-hidden>
      <ellipse cx="40" cy="72" rx="16" ry="5" fill="#000" opacity="0.2" />
      {leaves.map((l, i) => (
        <Leaf key={i} angle={l.angle} len={l.len} color={leaf} dark={leafDark} />
      ))}
      {showRoot && (
        <g>
          <ellipse cx="40" cy="68" rx="7.5" ry="5" fill="#e7c98f" stroke="#b9974f" strokeWidth="0.9" />
          <ellipse cx="40" cy="67" rx="3.5" ry="2" fill="#f3e2bb" opacity="0.8" />
        </g>
      )}
      {showHead && headR > 0 && (
        <g>
          {/* curd head: clustered creamy blobs */}
          <circle cx="40" cy={66 - headR * 0.4} r={headR} fill="#f1efe0" stroke="#cfc9a8" strokeWidth="1" />
          <circle cx={40 - headR * 0.4} cy={66 - headR * 0.5} r={headR * 0.45} fill="#fbfaf0" opacity="0.9" />
          <circle cx={40 + headR * 0.4} cy={66 - headR * 0.3} r={headR * 0.4} fill="#e8e5d2" opacity="0.85" />
        </g>
      )}
    </svg>
  );
}
