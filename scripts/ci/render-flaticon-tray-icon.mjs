import sharp from "sharp";
import fs from "fs";
import path from "path";

const src = "C:/Users/wanghuanyu/Downloads/animal.png";
const root = "C:/Users/wanghuanyu/github/AltTabBridge/windows-host/assets";
const sizes = [16, 20, 24, 32, 48, 256];

async function recolorGlyph(size) {
  // Load source, recolor opaque dark strokes to light, keep alpha
  const { data, info } = await sharp(src)
    .ensureAlpha()
    .resize(size, size, { fit: "contain", background: { r: 0, g: 0, b: 0, alpha: 0 } })
    .raw()
    .toBuffer({ resolveWithObject: true });

  const out = Buffer.alloc(data.length);
  for (let i = 0; i < data.length; i += 4) {
    const r = data[i];
    const g = data[i + 1];
    const b = data[i + 2];
    const a = data[i + 3];
    if (a < 8) {
      out[i] = out[i + 1] = out[i + 2] = 0;
      out[i + 3] = 0;
      continue;
    }
    // original is near-black strokes; recolor to soft white for dark tray badge
    const lum = (r + g + b) / 3;
    if (lum < 200) {
      out[i] = 244;
      out[i + 1] = 248;
      out[i + 2] = 255;
      out[i + 3] = a;
    } else {
      out[i] = r;
      out[i + 1] = g;
      out[i + 2] = b;
      out[i + 3] = a;
    }
  }
  return sharp(out, { raw: info }).png().toBuffer();
}

async function render(size) {
  const glyphBox = Math.round(size * 0.72);
  const glyph = await recolorGlyph(glyphBox);
  const bg = Buffer.from(
    `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}">
      <rect width="${size}" height="${size}" rx="${Math.round(size * 0.22)}" fill="#1C2A40"/>
    </svg>`
  );
  const left = Math.round((size - glyphBox) / 2);
  const top = Math.round((size - glyphBox) / 2);
  return sharp(bg)
    .composite([{ input: glyph, left, top }])
    .png()
    .toBuffer();
}

function pngsToIco(pngBuffers, dims) {
  const headerSize = 6 + 16 * pngBuffers.length;
  let offset = headerSize;
  const entries = [];
  for (let i = 0; i < pngBuffers.length; i++) {
    const dim = dims[i];
    entries.push({
      w: dim >= 256 ? 0 : dim,
      h: dim >= 256 ? 0 : dim,
      bytes: pngBuffers[i].length,
      offset,
    });
    offset += pngBuffers[i].length;
  }
  const header = Buffer.alloc(6);
  header.writeUInt16LE(0, 0);
  header.writeUInt16LE(1, 2);
  header.writeUInt16LE(pngBuffers.length, 4);
  const dir = Buffer.alloc(16 * pngBuffers.length);
  entries.forEach((e, i) => {
    const o = i * 16;
    dir.writeUInt8(e.w, o);
    dir.writeUInt8(e.h, o + 1);
    dir.writeUInt8(0, o + 2);
    dir.writeUInt8(0, o + 3);
    dir.writeUInt16LE(1, o + 4);
    dir.writeUInt16LE(32, o + 6);
    dir.writeUInt32LE(e.bytes, o + 8);
    dir.writeUInt32LE(e.offset, o + 12);
  });
  return Buffer.concat([header, dir, ...pngBuffers]);
}

const bufs = [];
for (const s of sizes) {
  bufs.push(await render(s));
  console.log("ok", s);
}
const ico = pngsToIco(bufs, sizes);
const icoPath = path.join(root, "workbuddy-host.ico");
fs.writeFileSync(icoPath, ico);
// keep source attribution copy
fs.copyFileSync(src, path.join(root, "workbuddy-buddy-source.png"));
console.log("wrote", icoPath, ico.length);
