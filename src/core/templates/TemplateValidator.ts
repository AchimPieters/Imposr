import { z } from 'zod';
import { TemplateError } from '@utils/errors';

/** Supported imposition modes for templates. */
export const templateModeSchema = z.enum(['sequential', 'booklet']);

/** Validation schema for template files. */
export const templateSchema = z.object({
  id: z
    .string()
    .min(1)
    .regex(/^[A-Za-z0-9_-]+$/, 'Template id may only contain letters, numbers, "_" and "-"'),
  name: z.string().min(1),
  mode: templateModeSchema,
  layout: z.object({
    columns: z.number().int().positive(),
    rows: z.number().int().positive(),
    gap: z.number().min(0),
    margin: z.number().min(0),
    sheetWidth: z.number().positive(),
    sheetHeight: z.number().positive(),
  }),
});

/** Runtime template type inferred from schema. */
export type ImpositionTemplate = z.infer<typeof templateSchema>;

/**
 * Validates template payloads before use/storage.
 */
export class TemplateValidator {
  /**
   * Validate unknown payload and return strongly typed template.
   * @throws TemplateError when payload is invalid.
   */
  validate(payload: unknown): ImpositionTemplate {
    const result = templateSchema.safeParse(payload);
    if (!result.success) {
      throw new TemplateError('Invalid template payload', {
        issues: result.error.issues.map((issue) => issue.message),
      });
    }
    return result.data;
  }
}
