import { TemplateLibrary } from '@core/templates/TemplateLibrary';
import { ImpositionTemplate } from '@core/templates/TemplateValidator';

/** Options for listing templates. */
export interface TemplateListOptions {
  directory: string;
}

/** Options for loading templates. */
export interface TemplateLoadOptions {
  directory: string;
  fileName: string;
}

/** Options for saving templates. */
export interface TemplateSaveOptions {
  directory: string;
  template: ImpositionTemplate;
}

/** List template files from a templates directory. */
export async function runTemplateListCommand(options: TemplateListOptions): Promise<string[]> {
  const library = new TemplateLibrary(options.directory);
  return library.list();
}

/** Load a concrete template from storage. */
export async function runTemplateLoadCommand(options: TemplateLoadOptions): Promise<ImpositionTemplate> {
  const library = new TemplateLibrary(options.directory);
  return library.load(options.fileName);
}

/** Persist a template to storage after schema validation. */
export async function runTemplateSaveCommand(options: TemplateSaveOptions): Promise<string> {
  const library = new TemplateLibrary(options.directory);
  return library.save(options.template);
}
