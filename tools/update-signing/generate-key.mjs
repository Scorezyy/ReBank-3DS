import { generateKeyPairSync } from "node:crypto";
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";

const [privateKeyPath, makefilePath] = process.argv.slice(2);
if (!privateKeyPath || !makefilePath) {
  console.error("usage: node generate-key.mjs <private-key.pem> <app/config/update-key.mk>");
  process.exit(1);
}
if (existsSync(privateKeyPath)) {
  console.error(`${privateKeyPath} already exists - refusing to overwrite a signing key.`);
  process.exit(1);
}

const { privateKey, publicKey } = generateKeyPairSync("rsa", { modulusLength: 2048, publicExponent: 65537 });
const modulus = Buffer.from(publicKey.export({ format: "jwk" }).n, "base64url").toString("hex");

mkdirSync(dirname(privateKeyPath), { recursive: true });
writeFileSync(privateKeyPath, privateKey.export({ format: "pem", type: "pkcs8" }), { mode: 0o600 });
writeFileSync(makefilePath, `REBANK_UPDATE_KEY_MODULUS := ${modulus}\n`);
console.log(`Private key written to ${privateKeyPath} - keep it offline and backed up.`);
console.log(`Public modulus written to ${makefilePath}.`);
