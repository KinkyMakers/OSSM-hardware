import { Command } from '@oclif/core';
import { execSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// ANSI color codes
const c = {
  reset: '\x1b[0m',
  bold: '\x1b[1m',
  dim: '\x1b[2m',
  red: '\x1b[31m',
  green: '\x1b[32m',
  yellow: '\x1b[33m',
  cyan: '\x1b[36m',
  gray: '\x1b[90m',
};

export default class ValidateLinks extends Command {
  static description = 'Validate mint docs and check broken links against production site';

  static examples = [
    '<%= config.bin %> <%= command.id %>',
  ];

  /**
   * Run mint validate
   */
  runMintValidate() {
    try {
      this.log(`${c.dim}Running mint validate...${c.reset}`);
      execSync('mint validate', { 
        stdio: 'inherit',
        cwd: join(__dirname, '..', '..'),
      });
      this.log(`${c.green}✓ mint validate passed${c.reset}\n`);
      return true;
    } catch (error) {
      this.error(`${c.red}✗ mint validate failed${c.reset}`, { exit: 1 });
    }
  }

  /**
   * Run mint broken-links and capture output
   */
  getBrokenLinks() {
    this.log(`${c.dim}Running mint broken-links...${c.reset}`);
    try {
      const output = execSync('mint broken-links', { 
        encoding: 'utf-8',
        cwd: join(__dirname, '..', '..'),
      });
      // If it succeeds, there are no broken links
      return '';
    } catch (error) {
      // mint broken-links exits with non-zero if broken links are found
      // We want to capture the output even if it fails
      // Combine stdout and stderr as mint may output to either
      const stdout = error.stdout?.toString() || '';
      const stderr = error.stderr?.toString() || '';
      const combined = (stdout + stderr).trim();
      
      // If we got output, return it; otherwise return empty string
      return combined || '';
    }
  }

  /**
   * Resolve a relative path against a file's directory
   * e.g., resolveRelativePath('../dashboard/subscription', 'dtt/faqs.mdx') 
   *       => '/dashboard/subscription'
   */
  resolveRelativePath(linkPath, filePath) {
    // If it's already absolute, return as-is
    if (linkPath.startsWith('/')) {
      return linkPath;
    }
    
    // Get the directory of the file (e.g., 'dtt/faqs.mdx' -> '/dtt')
    const fileDir = '/' + filePath.replace(/\/[^/]+$/, '');
    
    // Combine and normalize the path
    const combined = fileDir + '/' + linkPath;
    
    // Resolve .. and . in the path
    const parts = combined.split('/');
    const resolved = [];
    
    for (const part of parts) {
      if (part === '' || part === '.') {
        continue;
      } else if (part === '..') {
        resolved.pop();
      } else {
        resolved.push(part);
      }
    }
    
    return '/' + resolved.join('/');
  }

  /**
   * Parse broken links from mint output, preserving file groupings
   * Returns: { files: Map<string, Array<{original, resolved}>>, totalLinks: number, fileCount: number }
   */
  parseBrokenLinksGrouped(output) {
    const files = new Map(); // filename -> array of { original, resolved }
    const lines = output.split('\n');
    
    let currentFile = null;
    
    for (const line of lines) {
      const trimmed = line.trim();
      
      // Skip empty lines
      if (!trimmed) continue;
      
      // Skip the summary line (e.g., "found 7 broken links in 3 files")
      if (trimmed.startsWith('found ') && trimmed.includes(' broken links')) {
        continue;
      }
      
      // Check if this is a file line (ends with .mdx or .md, no leading symbol)
      if ((trimmed.endsWith('.mdx') || trimmed.endsWith('.md')) && !trimmed.startsWith('⎿')) {
        currentFile = trimmed;
        if (!files.has(currentFile)) {
          files.set(currentFile, []);
        }
        continue;
      }
      
      // Check if this is a link line (starts with ⎿)
      if (currentFile) {
        // Match the link path after ⎿ (absolute paths starting with / or relative paths)
        const linkMatch = trimmed.match(/^⎿\s*([^\s]+)/);
        if (linkMatch) {
          const original = linkMatch[1];
          const resolved = this.resolveRelativePath(original, currentFile);
          files.get(currentFile).push({ original, resolved });
          continue;
        }
        
        // Also handle lines that are just paths (absolute or relative)
        const pathMatch = trimmed.match(/^([.\/][^\s]+)$/);
        if (pathMatch) {
          const original = pathMatch[1];
          const resolved = this.resolveRelativePath(original, currentFile);
          files.get(currentFile).push({ original, resolved });
        }
      }
    }
    
    // Count total links
    let totalLinks = 0;
    for (const links of files.values()) {
      totalLinks += links.length;
    }
    
    return { files, totalLinks, fileCount: files.size };
  }

  /**
   * Check if a link exists on production
   * Returns { exists: boolean, url: string }
   */
  async checkLinkExists(link) {
    const productionBase = 'https://docs.researchanddesire.com';
    const urlToCheck = `${productionBase}${link}`;
    
    try {
      const response = await fetch(urlToCheck, { 
        method: 'GET',
        redirect: 'follow',
      });
      
      // Must have 200 status code
      if (response.status !== 200) {
        return { exists: false, url: urlToCheck };
      }
      
      // Check response body for client-side 404 indicators
      const text = await response.text();
      
      // Extract and check the title tag (case-insensitive)
      const titleMatch = text.match(/<title[^>]*>([^<]*)<\/title>/i);
      const title = titleMatch ? titleMatch[1].toLowerCase() : '';
      
      // Check if title indicates a 404 page
      // Real 404 pages have titles like "404", "Page Not Found", "Not Found", etc.
      const titleHas404 = 
        title.includes('404') ||
        title.includes('page not found') ||
        title.includes('not found') ||
        title === '';
      
      return { exists: !titleHas404, url: urlToCheck };
    } catch (error) {
      // Network error or other issue - treat as not found
      return { exists: false, url: urlToCheck, error: error.message };
    }
  }

  async run() {
    // First run mint validate
    this.runMintValidate();
    
    // Get broken links
    const output = this.getBrokenLinks();
    
    if (!output || output.trim().length === 0) {
      this.log(`${c.green}✓ No broken links found${c.reset}`);
      return;
    }
    
    // Parse broken links with file groupings
    const { files, totalLinks, fileCount } = this.parseBrokenLinksGrouped(output);
    
    if (totalLinks === 0) {
      this.log(`${c.green}✓ No broken links to validate${c.reset}`);
      return;
    }
    
    this.log(`\n${c.cyan}Checking ${totalLinks} link(s) against production...${c.reset}\n`);
    
    // Check each link and store results
    // Map: filename -> array of { original, resolved, exists, url }
    const results = new Map();
    let brokenCount = 0;
    let externalCount = 0;
    
    for (const [file, links] of files) {
      const fileResults = [];
      
      for (const { original, resolved } of links) {
        const result = await this.checkLinkExists(resolved);
        fileResults.push({
          original,
          resolved,
          exists: result.exists,
          url: result.url,
        });
        
        if (result.exists) {
          externalCount++;
        } else {
          brokenCount++;
        }
      }
      
      results.set(file, fileResults);
    }
    
    // Output in the desired format with colors
    if (brokenCount > 0) {
      this.log(`${c.red}${c.bold}found ${brokenCount} broken links${c.reset}${c.dim} in ${fileCount} files${c.reset}`);
    } else {
      this.log(`${c.green}found ${brokenCount} broken links${c.reset}${c.dim} in ${fileCount} files${c.reset}`);
    }
    this.log(`${c.green}found ${externalCount} external links${c.reset}${c.dim} in ${fileCount} files${c.reset}`);
    this.log('');
    this.log('');
    
    for (const [file, fileResults] of results) {
      this.log(`${c.cyan}${c.bold}${file}${c.reset}`);
      
      for (const { original, resolved, exists, url } of fileResults) {
        // Show resolved path if different from original
        const pathDisplay = original !== resolved 
          ? `${original} ${c.dim}→${c.reset} ${resolved}`
          : original;
        
        if (exists) {
          this.log(` ${c.dim}⎿${c.reset}  ${c.bold}${pathDisplay}${c.reset} ${c.gray}-${c.reset} ${c.green}exists at${c.reset} ${c.dim}${url}${c.reset}`);
        } else {
          this.log(` ${c.dim}⎿${c.reset}  ${c.bold}${pathDisplay}${c.reset} ${c.gray}-${c.reset} ${c.red}${c.bold}broken${c.reset}`);
        }
      }
      
      this.log('');
    }
    
    // Exit with error if there are broken links
    if (brokenCount > 0) {
      this.error(`${c.red}Link validation failed${c.reset}`, { exit: 1 });
    }
  }
}
