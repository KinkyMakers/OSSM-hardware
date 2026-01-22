import { useState, useEffect, useRef } from 'react';



export const DeviceFlasher = ({ device = 'ossm' }) => {
  const DEVICE_CONFIGS = {
    ossm: {
      name: 'OSSM',
      fullName: 'Open Source Sex Machine',
      storageBucket: 'ossm-firmware',
      productionManifestPath: 'master/manifest.json',
      description: 'Flash your Open Source Sex Machine with the latest firmware. Connect via USB-C to get started.',
      connectInstructions: 'Connect your OSSM to your computer via USB-C',
    },
    radr: {
      name: 'RADR',
      fullName: 'RAD Wireless Remote',
      storageBucket: 'radr-firmware',
      productionManifestPath: 'master/manifest.json',
      description: 'Flash your RADR wireless remote with the latest firmware. Connect via USB-C to get started.',
      connectInstructions: 'Connect your RADR to your computer via USB-C',
    },
  };

  const STORAGE_BASE_URL = 'https://acjajruwevyyatztbkdf.supabase.co/storage/v1/object/public';
  const config = DEVICE_CONFIGS[device] || DEVICE_CONFIGS.ossm;
  const STORAGE_URL = `${STORAGE_BASE_URL}/${config.storageBucket}`;

  const [mode, setMode] = useState('production');
  const [branches, setBranches] = useState([]);
  const [selectedBranch, setSelectedBranch] = useState('');
  const [selectedCommit, setSelectedCommit] = useState('head');
  const [isLoadingBranches, setIsLoadingBranches] = useState(false);
  const [branchError, setBranchError] = useState(null);

  const [manifestUrl, setManifestUrl] = useState('');
  const [isLoadingManifest, setIsLoadingManifest] = useState(false);
  const [manifestError, setManifestError] = useState(null);

  const espToolsLoaded = useRef(false);

  // Load ESP Web Tools from CDN
  useEffect(() => {
    if (espToolsLoaded.current) return;

    const script = document.createElement('script');
    script.type = 'module';
    script.defer = true;
    script.src = 'https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module';
    document.head.appendChild(script);
    espToolsLoaded.current = true;

    return () => {
      // Cleanup not needed - script stays loaded
    };
  }, []);

  // Fetch branches from registry
  useEffect(() => {
    const fetchBranches = async () => {
      setIsLoadingBranches(true);
      setBranchError(null);
      try {
        const response = await fetch(`${STORAGE_URL}/registry.json`);
        if (!response.ok) throw new Error('Failed to fetch branches');

        const registry = await response.json();
        const branchList = Object.entries(registry).map(([branchPath, commits]) => ({
          path: branchPath,
          commits: ['head', ...commits],
        }));

        branchList.sort((a, b) => a.path.localeCompare(b.path));
        setBranches(branchList);

        if (branchList.length > 0) {
          setSelectedBranch(branchList[0].path);
        }
      } catch (error) {
        console.error('Error fetching branches:', error);
        setBranchError('Failed to load available branches');
        setBranches([]);
      } finally {
        setIsLoadingBranches(false);
      }
    };

    fetchBranches();
  }, [STORAGE_URL]);

  // Reset branch when switching modes
  useEffect(() => {
    if (mode === 'production') {
      setSelectedBranch('');
      setSelectedCommit('head');
    } else if (branches.length > 0 && !selectedBranch) {
      setSelectedBranch(branches[0].path);
    }
  }, [mode, branches]);

  // Reset commit when branch changes
  useEffect(() => {
    setSelectedCommit('head');
  }, [selectedBranch]);

  // Update manifest URL
  useEffect(() => {
    setIsLoadingManifest(true);
    setManifestError(null);

    const checkManifest = async () => {
      try {
        let manifestPath;

        if (mode === 'production') {
          manifestPath = config.productionManifestPath;
        } else {
          if (!selectedBranch) {
            setManifestError('Please select a branch');
            setIsLoadingManifest(false);
            return;
          }

          manifestPath = selectedCommit === 'head'
            ? `${selectedBranch}/manifest.json`
            : `${selectedBranch}/${selectedCommit}/manifest.json`;
        }

        const manifestFullUrl = `${STORAGE_URL}/${manifestPath}`;

        const manifestResponse = await fetch(manifestFullUrl, { method: 'HEAD' });

        if (!manifestResponse.ok) {
          const errorContext = mode === 'production'
            ? 'production firmware'
            : `${decodeURIComponent(selectedBranch)}${selectedCommit !== 'head' ? ` (${selectedCommit})` : ''}`;
          setManifestError(`Manifest not found for ${errorContext}`);
          setIsLoadingManifest(false);
          return;
        }

        setManifestUrl(manifestFullUrl);
      } catch (error) {
        console.error('Error checking manifest:', error);
        setManifestError('Error checking manifest availability');
      } finally {
        setIsLoadingManifest(false);
      }
    };

    checkManifest();
  }, [mode, selectedBranch, selectedCommit, STORAGE_URL, config.productionManifestPath]);

  const selectedBranchData = branches.find((b) => b.path === selectedBranch);
  const commits = selectedBranchData?.commits ?? ['head'];

  return (
    <div className="not-prose mx-auto max-w-2xl rounded-xl border border-zinc-200 bg-white p-6 dark:border-zinc-700 dark:bg-zinc-900">
      {/* Header */}
      <div className="mb-4">
        <h2 className="text-xl font-semibold text-zinc-900 dark:text-zinc-100">
          {config.name} Web Flasher
        </h2>
        <p className="mt-1 text-sm text-zinc-500 dark:text-zinc-400">
          {config.description}
        </p>
      </div>

      {/* Branch Error */}
      {branchError && (
        <div className="mb-4 rounded-lg border border-red-300 bg-red-50 p-3 dark:border-red-700 dark:bg-red-900/20">
          <p className="font-medium text-red-800 dark:text-red-300">Error</p>
          <p className="text-sm text-red-700 dark:text-red-400">{branchError}</p>
        </div>
      )}

      {/* Controls */}
      <div className="mb-4 flex flex-wrap gap-3">
        {/* Mode Selection */}
        <div className="flex flex-col gap-1.5">
          <label className="text-sm font-medium text-zinc-500 dark:text-zinc-400">
            Mode
          </label>
          <select
            value={mode}
            onChange={(e) => setMode(e.target.value)}
            className="w-[180px] rounded-lg border border-zinc-300 bg-white px-3 py-2 text-sm text-zinc-900 focus:border-violet-500 focus:outline-none focus:ring-1 focus:ring-violet-500 dark:border-zinc-600 dark:bg-zinc-800 dark:text-zinc-100"
          >
            <option value="production">Production (Stable)</option>
            <option value="development">Development</option>
          </select>
        </div>

        {/* Branch Selection */}
        {mode === 'development' && (
          <div className="flex flex-col gap-1.5">
            <label className="text-sm font-medium text-zinc-500 dark:text-zinc-400">
              Branch
            </label>
            <select
              value={selectedBranch}
              onChange={(e) => setSelectedBranch(e.target.value)}
              disabled={isLoadingBranches}
              className="w-[200px] rounded-lg border border-zinc-300 bg-white px-3 py-2 text-sm text-zinc-900 focus:border-violet-500 focus:outline-none focus:ring-1 focus:ring-violet-500 disabled:cursor-not-allowed disabled:bg-zinc-100 dark:border-zinc-600 dark:bg-zinc-800 dark:text-zinc-100 dark:disabled:bg-zinc-900"
            >
              {isLoadingBranches ? (
                <option>Loading branches...</option>
              ) : (
                branches.map((branch) => (
                  <option key={branch.path} value={branch.path}>
                    {branch.path ? decodeURIComponent(branch.path) : '(root)'}
                  </option>
                ))
              )}
            </select>
          </div>
        )}

        {/* Commit Selection */}
        {mode === 'development' && selectedBranch && (
          <div className="flex flex-col gap-1.5">
            <label className="text-sm font-medium text-zinc-500 dark:text-zinc-400">
              Commit
            </label>
            <select
              value={selectedCommit}
              onChange={(e) => setSelectedCommit(e.target.value)}
              className="w-[150px] rounded-lg border border-zinc-300 bg-white px-3 py-2 text-sm text-zinc-900 focus:border-violet-500 focus:outline-none focus:ring-1 focus:ring-violet-500 dark:border-zinc-600 dark:bg-zinc-800 dark:text-zinc-100"
            >
              {commits.map((commit) => (
                <option key={commit} value={commit}>
                  {commit === 'head' ? 'Head (Latest)' : commit}
                </option>
              ))}
            </select>
          </div>
        )}
      </div>

      {/* Development Info */}
      {mode === 'development' && selectedBranch && !manifestError && (
        <div className="mb-4 rounded-lg border border-zinc-300 bg-zinc-50 p-3 dark:border-zinc-600 dark:bg-zinc-800">
          <p className="font-medium text-zinc-800 dark:text-zinc-200">Development Firmware</p>
          <p className="text-sm text-zinc-600 dark:text-zinc-400">
            {`Branch: ${decodeURIComponent(selectedBranch)}${selectedCommit !== 'head' ? ` (${selectedCommit})` : ' (latest)'}`}
          </p>
        </div>
      )}

      {/* Flash Button Area */}
      <div className="mb-4 pt-2">
        {isLoadingManifest && (
          <div className="flex flex-col items-center justify-center py-4">
            <div className="h-8 w-8 animate-spin rounded-full border-4 border-zinc-200 border-t-violet-500 dark:border-zinc-700 dark:border-t-violet-400" />
            <p className="mt-2 text-sm text-zinc-500 dark:text-zinc-400">
              Checking firmware availability...
            </p>
          </div>
        )}

        {manifestError && (
          <div className="rounded-lg border border-red-300 bg-red-50 p-3 dark:border-red-700 dark:bg-red-900/20">
            <p className="font-medium text-red-800 dark:text-red-300">Firmware Not Available</p>
            <p className="text-sm text-red-700 dark:text-red-400">
              {manifestError}
              <br />
              <span className="text-xs">Please check the branch/commit and try again.</span>
            </p>
          </div>
        )}

        {!isLoadingManifest && !manifestError && manifestUrl && (
          <div
            dangerouslySetInnerHTML={{
              __html: `<esp-web-install-button manifest="${manifestUrl}">
                <button slot="activate" style="background-color: #8b5cf6; color: white; padding: 10px 24px; border-radius: 9999px; font-weight: 500; border: none; cursor: pointer; transition: background-color 0.2s;">
                  Connect &amp; Flash
                </button>
              </esp-web-install-button>`,
            }}
          />
        )}
      </div>

      {/* Instructions */}
      <div className="rounded-lg border border-zinc-200 bg-zinc-50 p-3 text-sm dark:border-zinc-700 dark:bg-zinc-800/50">
        <p className="font-medium text-zinc-800 dark:text-zinc-200">Instructions:</p>
        <ol className="mt-2 list-inside list-decimal space-y-1 text-zinc-600 dark:text-zinc-400">
          <li>{config.connectInstructions}</li>
          <li>Select the firmware version above</li>
          <li>Click the "Connect & Flash" button</li>
          <li>Select your {config.name} from the device list</li>
          <li>Wait for the flash to complete</li>
        </ol>
      </div>
    </div>
  );
};