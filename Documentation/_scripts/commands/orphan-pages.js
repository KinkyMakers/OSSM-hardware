import { Command, Flags } from '@oclif/core';
import { readFileSync, readdirSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';
import { fileURLToPath } from 'node:url';
import { dirname } from 'node:path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

export default class OrphanPages extends Command {
  static description = 'Find MDX files in ossm/ that are not listed in docs.json';

  static examples = [
    '<%= config.bin %> <%= command.id %>',
    '<%= config.bin %> <%= command.id %> --verbose',
  ];

  static flags = {
    verbose: Flags.boolean({
      char: 'v',
      description: 'Show all files, not just orphaned ones',
      default: false,
    }),
  };

  /**
   * Recursively find all .mdx files in a directory
   */
  findMdxFiles(dir, basePath = '') {
    const files = [];
    const entries = readdirSync(dir, { withFileTypes: true });

    for (const entry of entries) {
      const fullPath = join(dir, entry.name);
      const relativePath = basePath ? `${basePath}/${entry.name}` : entry.name;

      if (entry.isDirectory()) {
        files.push(...this.findMdxFiles(fullPath, relativePath));
      } else if (entry.isFile() && entry.name.endsWith('.mdx')) {
        // Convert to docs.json format: ossm/path/to/file (without .mdx extension)
        const docPath = `ossm/${relativePath.replace(/\.mdx$/, '')}`;
        files.push(docPath);
      }
    }

    return files;
  }

  /**
   * Recursively extract all page paths from docs.json navigation
   */
  extractPagesFromNav(obj) {
    const pages = new Set();

    if (typeof obj === 'string') {
      pages.add(obj);
    } else if (Array.isArray(obj)) {
      for (const item of obj) {
        for (const page of this.extractPagesFromNav(item)) {
          pages.add(page);
        }
      }
    } else if (typeof obj === 'object' && obj !== null) {
      // Handle 'pages' arrays directly
      if (obj.pages) {
        for (const page of this.extractPagesFromNav(obj.pages)) {
          pages.add(page);
        }
      }
      // Handle 'groups' arrays
      if (obj.groups) {
        for (const page of this.extractPagesFromNav(obj.groups)) {
          pages.add(page);
        }
      }
      // Handle 'tabs' arrays
      if (obj.tabs) {
        for (const page of this.extractPagesFromNav(obj.tabs)) {
          pages.add(page);
        }
      }
      // Handle 'products' arrays
      if (obj.products) {
        for (const page of this.extractPagesFromNav(obj.products)) {
          pages.add(page);
        }
      }
      // Handle 'navigation' object
      if (obj.navigation) {
        for (const page of this.extractPagesFromNav(obj.navigation)) {
          pages.add(page);
        }
      }
    }

    return pages;
  }

  async run() {
    const { flags } = await this.parse(OrphanPages);

    // Paths relative to the Documentation directory
    const docsRoot = join(__dirname, '..', '..');
    const ossmDir = join(docsRoot, 'ossm');
    const docsJsonPath = join(docsRoot, 'docs.json');

    // Find all MDX files in ossm/
    const mdxFiles = this.findMdxFiles(ossmDir);

    // Parse docs.json and extract all page references
    const docsJson = JSON.parse(readFileSync(docsJsonPath, 'utf-8'));
    const listedPages = this.extractPagesFromNav(docsJson);

    // Find orphaned files (in ossm/ but not in docs.json)
    const orphanedFiles = mdxFiles.filter((file) => !listedPages.has(file));

    if (flags.verbose) {
      this.log(`\nTotal MDX files found: ${mdxFiles.length}`);
      this.log(`Pages listed in docs.json: ${listedPages.size}`);
      this.log(`Orphaned files: ${orphanedFiles.length}\n`);
    }

    if (orphanedFiles.length === 0) {
      this.log('✓ No orphaned MDX files found. All files are listed in docs.json.');
      return;
    }

    this.log(`\n⚠ Found ${orphanedFiles.length} orphaned MDX file(s):\n`);
    for (const file of orphanedFiles.sort()) {
      this.log(`  - ${file}`);
    }
    this.log('');

    // Exit with error code if orphans found (useful for CI)
    this.exit(1);
  }
}
