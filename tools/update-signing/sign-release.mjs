import { createPrivateKey, createPublicKey, sign, verify } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";

const [privateKeyPath, ...files] = process.argv.slice(2);
if (!privateKeyPath || files.length === 0) {
  console.error("usage: node sign-release.mjs <private-key.pem> ReBank.cia ReBank.3dsx");
  process.exit(1);
}

const privateKey = createPrivateKey(readFileSync(privateKeyPath));
const publicKey = createPublicKey(privateKey);
for (const file of files) {
  const data = readFileSync(file);
  const signature = sign("sha256", data, privateKey);
  if (!verify("sha256", data, publicKey, signature)) {
    throw new Error(`Signature self-check failed for ${file}`);
  }
  writeFileSync(`${file}.sig`, `${signature.toString("base64")}\n`);
  console.log(`${file}.sig written - upload it to the GitHub release next to ${file}.`);
}
