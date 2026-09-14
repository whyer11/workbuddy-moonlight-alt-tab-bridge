import sharp from "sharp";
import fs from "fs";
import path from "path";

const root = "C:/Users/wanghuanyu/github/AltTabBridge/windows-host/assets";
const svgPath = path.join(root, "lucide-copy.svg");
const sizes = [16, 20, 24, 32, 48, 256];

let svg = fs.readFileSync(svgPath, "utf8");
// Lucide uses currentColor; force a light glyph for dark taskbars
svg = svg
  .replace(/currentColor/g, "#F4F8FF")
  .replace(/stroke-width="2"/g, 'stroke-width="1.75"');

async function render(size) {
  const padRatio = 0.12;
  const pad = Math.round(size * padRatio);
  const inner = size - pad * 2;
  const glyph = await sharp(Buffer.from(svg))
    .resize(Math.max(1, Math.round(inner * 0.78)), Math.max(1, Math.round(inner * 0.78)), {
      fit: "contain",
      background: { r: 0, g: 0, b: 0, alpha: 0 },
    })
    .png()
    .toBuffer();

  const bg = Buffer.from(
    `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}">
      <rect x="0" y="0" width="${size}" height="${size}" rx="${Math.round(size * 0.22)}"
        fill="#1C2A40"/>
    </svg>`
  );

  const glyphSize = Math.max(1, Math.round(inner * 0.78));
  const left = Math.round((size - glyphSize) / 2);
  const top = Math.round((size - glyphSize) / 2);

  const composed = await sharp(bg)
    .composite([{ input: glyph, left, top }])
    .png()
    .toBuffer();

  const out = path.join(root, `tray-${size}.png`);
  fs.writeFileSync(out, composed);
  return out;
}

const outs = [];
for (const s of sizes) {
  outs.push(await render(s));
  console.log("rendered", s);
}

// Build ICO manually (PNG-compressed entries, Vista+)
function pngToIco(pngPaths) {
  const images = pngPaths.map((p) => fs.readFileSync(p));
  const headerSize = 6 + 16 * images.length;
  let offset = headerSize;
  const entries = [];
  const payloads = [];
  for (let i = 0; i < images.length; i++) {
    const buf = images[i];
    const dim = sizes[i];
    // parse IHDR for width/height if needed; use known size
    const w = dim >= 256 ? 0 : dim;
    const h = dim >= 256 ? 0 : dim;
    entries.push({
      w,
      h,
      planes: 1,
      bitCount: 32,
      bytes: buf.length,
      offset,
    });
    payloads.push(buf);
    offset += buf.length;
  }
  const header = Buffer.alloc(6);
  header.writeUInt16LE(0, 0);
  header.writeUInt16LE(1, 2); // icon
  header.writeUInt16LE(images.length, 4);
  const dir = Buffer.alloc(16 * images.length);
  entries.forEach((e, i) => {
    const o = i * 16;
    dir.writeUInt8(e.w, o);
    dir.writeUInt8(e.h, o + 1);
    dir.writeUInt8(0, o + 2); // palette
    dir.writeUInt8(0, o + 3); // reserved
    dir.writeUInt16LE(e.planes, o + 4);
    dir.writeUInt16LE(e.bitCount, o + 6);
    dir.writeUInt32LE(e.bytes, o + 8);
    dir.writeUInt32LE(e.offset, o + 12);
  });
  return Buffer.concat([header, dir, ...payloads]);
}

const ico = pngToIco(outs);
const icoPath = path.join(root, "workbuddy-host.ico");
fs.writeFileSync(icoPath, ico);
console.log("wrote", icoPath, ico.length);
