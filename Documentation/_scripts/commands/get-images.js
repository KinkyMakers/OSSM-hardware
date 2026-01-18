import { Command, Flags } from '@oclif/core';
import { confirm } from '@inquirer/prompts';
import { readFileSync, readdirSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import { join, dirname, basename, extname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { dirname as pathDirname } from 'node:path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = pathDirname(__filename);

export default class GetImages extends Command {
  static description = 'Find external images in MDX files and optionally download them locally';

  static examples = [
    '<%= config.bin %> <%= command.id %>',
    '<%= config.bin %> <%= command.id %> --yes',
    '<%= config.bin %> <%= command.id %> --dry-run',
  ];

  static flags = {
    yes: Flags.boolean({
      char: 'y',
      description: 'Automatically download all external images without prompting',
      default: false,
    }),
    'dry-run': Flags.boolean({
      char: 'd',
      description: 'Show what would be done without making changes',
      default: false,
    }),
  };

  /**
   * Recursively find all .mdx files in a directory
   */
  findMdxFiles(dir) {
    const files = [];
    const entries = readdirSync(dir, { withFileTypes: true });

    for (const entry of entries) {
      const fullPath = join(dir, entry.name);

      if (entry.isDirectory()) {
        // Skip _images directories
        if (entry.name === '_images') continue;
        files.push(...this.findMdxFiles(fullPath));
      } else if (entry.isFile() && entry.name.endsWith('.mdx')) {
        files.push(fullPath);
      }
    }

    return files;
  }

  /**
   * Find all <img /> tags with external src attributes in content
   */
  findExternalImages(content) {
    const images = [];
    // Match <img ... src="..." ... /> or <img ... src="..." ...>
    // Also match <Image ... src="..." ... /> (Mintlify/Next.js Image component)
    const imgRegex = /<(?:img|Image)\s+[^>]*src\s*=\s*["']([^"']+)["'][^>]*\/?>/gi;
    
    let match;
    while ((match = imgRegex.exec(content)) !== null) {
      const src = match[1];
      // Check if it's an external URL
      if (src.startsWith('http://') || src.startsWith('https://')) {
        images.push({
          fullMatch: match[0],
          src: src,
          index: match.index,
        });
      }
    }

    return images;
  }

  /**
   * Extract filename from URL, handling query strings and generating safe filenames
   */
  getFilenameFromUrl(url) {
    try {
      const urlObj = new URL(url);
      let pathname = urlObj.pathname;
      
      // Get the filename from the path
      let filename = basename(pathname);
      
      // If no extension, try to infer from URL or default to .png
      if (!extname(filename)) {
        // Check query params for format hints
        const format = urlObj.searchParams.get('format') || urlObj.searchParams.get('f');
        if (format) {
          filename = `${filename}.${format}`;
        } else {
          filename = `${filename}.png`;
        }
      }
      
      // Make filename safe (remove special chars, limit length)
      filename = filename
        .replace(/[^a-zA-Z0-9._-]/g, '_')
        .substring(0, 100);
      
      return filename;
    } catch {
      // Fallback for malformed URLs
      return `image_${Date.now()}.png`;
    }
  }

  /**
   * Download an image from URL and save it locally
   */
  async downloadImage(url, destPath) {
    const response = await fetch(url);
    
    if (!response.ok) {
      throw new Error(`Failed to download: ${response.status} ${response.statusText}`);
    }
    
    const buffer = Buffer.from(await response.arrayBuffer());
    writeFileSync(destPath, buffer);
    
    return buffer.length;
  }

  /**
   * Generate a unique filename if one already exists
   */
  getUniqueFilename(dir, filename) {
    const ext = extname(filename);
    const base = basename(filename, ext);
    let finalName = filename;
    let counter = 1;

    while (existsSync(join(dir, finalName))) {
      finalName = `${base}_${counter}${ext}`;
      counter++;
    }

    return finalName;
  }

  async run() {
    const { flags } = await this.parse(GetImages);
    const dryRun = flags['dry-run'];

    // Paths relative to the Documentation directory
    const docsRoot = join(__dirname, '..', '..');
    const ossmDir = join(docsRoot, 'ossm');

    // Find all MDX files
    this.log('Scanning for MDX files...');
    const mdxFiles = this.findMdxFiles(ossmDir);
    this.log(`Found ${mdxFiles.length} MDX files\n`);

    let totalExternalImages = 0;
    let totalDownloaded = 0;
    let totalSkipped = 0;

    for (const mdxPath of mdxFiles) {
      const content = readFileSync(mdxPath, 'utf-8');
      const externalImages = this.findExternalImages(content);

      if (externalImages.length === 0) continue;

      const relativePath = mdxPath.replace(docsRoot + '/', '');
      this.log(`\n📄 ${relativePath}`);
      this.log(`   Found ${externalImages.length} external image(s):`);

      totalExternalImages += externalImages.length;

      // Track replacements to make
      const replacements = [];

      for (const img of externalImages) {
        // Truncate URL for display
        const displayUrl = img.src.length > 60 
          ? img.src.substring(0, 57) + '...' 
          : img.src;
        
        this.log(`\n   🔗 ${displayUrl}`);

        let shouldDownload = flags.yes;

        if (!shouldDownload && !dryRun) {
          shouldDownload = await confirm({ message: 'Download this image?' });
        }

        if (dryRun) {
          this.log('   ⏭️  [dry-run] Would prompt to download');
          continue;
        }

        if (!shouldDownload) {
          this.log('   ⏭️  Skipped');
          totalSkipped++;
          continue;
        }

        try {
          // Create _images directory next to the MDX file
          const mdxDir = dirname(mdxPath);
          const imagesDir = join(mdxDir, '_images');

          if (!existsSync(imagesDir)) {
            mkdirSync(imagesDir, { recursive: true });
            this.log(`   📁 Created ${imagesDir.replace(docsRoot + '/', '')}`);
          }

          // Download the image
          const filename = this.getFilenameFromUrl(img.src);
          const uniqueFilename = this.getUniqueFilename(imagesDir, filename);
          const destPath = join(imagesDir, uniqueFilename);

          this.log(`   ⬇️  Downloading...`);
          const size = await this.downloadImage(img.src, destPath);
          this.log(`   ✅ Saved ${uniqueFilename} (${(size / 1024).toFixed(1)} KB)`);

          // Track replacement: new src should be relative path
          const newSrc = `./_images/${uniqueFilename}`;
          const newTag = img.fullMatch.replace(img.src, newSrc);
          
          replacements.push({
            oldTag: img.fullMatch,
            newTag: newTag,
          });

          totalDownloaded++;
        } catch (error) {
          this.log(`   ❌ Error: ${error.message}`);
          totalSkipped++;
        }
      }

      // Apply all replacements to the file
      if (replacements.length > 0) {
        let updatedContent = content;
        for (const { oldTag, newTag } of replacements) {
          updatedContent = updatedContent.replace(oldTag, newTag);
        }
        writeFileSync(mdxPath, updatedContent, 'utf-8');
        this.log(`\n   📝 Updated ${replacements.length} image reference(s) in file`);
      }
    }

    // Summary
    this.log('\n' + '─'.repeat(50));
    this.log('Summary:');
    this.log(`  Total external images found: ${totalExternalImages}`);
    if (!dryRun) {
      this.log(`  Downloaded: ${totalDownloaded}`);
      this.log(`  Skipped: ${totalSkipped}`);
    } else {
      this.log('  [dry-run mode - no changes made]');
    }

    if (totalExternalImages === 0) {
      this.log('\n✓ No external images found in MDX files.');
    }
  }
}
