import { createCipheriv, createDecipheriv, createHash, randomBytes } from 'node:crypto';
import { LicenseError } from '@utils/errors';

interface EncryptedEnvelope {
  v: 1;
  alg: 'aes-256-gcm';
  iv: string;
  tag: string;
  data: string;
}

export interface FileCodec {
  encode(raw: string): string;
  decode(raw: string): string;
  readonly enabled: boolean;
}

class PlaintextFileCodec implements FileCodec {
  readonly enabled = false;

  encode(raw: string): string {
    return raw;
  }

  decode(raw: string): string {
    return raw;
  }
}

class AesGcmFileCodec implements FileCodec {
  readonly enabled = true;
  private readonly key: Buffer;

  constructor(secret: string) {
    this.key = createHash('sha256').update(secret, 'utf8').digest();
  }

  encode(raw: string): string {
    const iv = randomBytes(12);
    const cipher = createCipheriv('aes-256-gcm', this.key, iv);
    const encrypted = Buffer.concat([cipher.update(raw, 'utf8'), cipher.final()]);
    const envelope: EncryptedEnvelope = {
      v: 1,
      alg: 'aes-256-gcm',
      iv: iv.toString('base64'),
      tag: cipher.getAuthTag().toString('base64'),
      data: encrypted.toString('base64'),
    };
    return JSON.stringify(envelope);
  }

  decode(raw: string): string {
    let envelope: EncryptedEnvelope;
    try {
      envelope = JSON.parse(raw) as EncryptedEnvelope;
    } catch {
      throw new LicenseError('Encrypted file payload is malformed JSON');
    }

    if (envelope.v !== 1 || envelope.alg !== 'aes-256-gcm') {
      throw new LicenseError('Unsupported encrypted file payload format');
    }

    try {
      const decipher = createDecipheriv(
        'aes-256-gcm',
        this.key,
        Buffer.from(envelope.iv, 'base64')
      );
      decipher.setAuthTag(Buffer.from(envelope.tag, 'base64'));
      const decrypted = Buffer.concat([
        decipher.update(Buffer.from(envelope.data, 'base64')),
        decipher.final(),
      ]);
      return decrypted.toString('utf8');
    } catch (error) {
      throw new LicenseError('Unable to decrypt file payload', {
        cause: error instanceof Error ? error.message : 'unknown',
      });
    }
  }
}

export function createFileCodec(secret?: string): FileCodec {
  if (!secret || secret.trim().length === 0) {
    return new PlaintextFileCodec();
  }

  if (secret.length < 16) {
    throw new LicenseError('Encryption secret must be at least 16 characters');
  }

  return new AesGcmFileCodec(secret);
}
