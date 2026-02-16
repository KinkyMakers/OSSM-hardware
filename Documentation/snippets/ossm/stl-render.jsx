export const STLViewer = ({ url }) => {
  const [loading, setLoading] = useState(true);
  
  // ViewSTL.com embed service - designed for embedding STL files
  const EMBED_URL = `https://www.viewstl.com/?embedded&url=${encodeURIComponent(url)}`;

  // Convert raw GitHub URL to regular GitHub URL for the link
  const getGitHubUrl = (rawUrl) => {
    if (rawUrl.includes('raw.githubusercontent.com')) {
      return rawUrl
        .replace('raw.githubusercontent.com', 'github.com')
        .replace(/\/main\//, '/blob/main/');
    }
    return rawUrl;
  };

  const fileUrl = getGitHubUrl(url);
  const fileName = decodeURIComponent(url.split('/').pop());

  return (
    <div style={{ width: '100%', maxWidth: '800px', margin: '0 auto' }}>
      {loading && (
        <div style={{ 
          position: 'absolute',
          display: 'flex', 
          alignItems: 'center', 
          justifyContent: 'center',
          width: '100%',
          height: '500px',
          background: '#f5f5f5',
          borderRadius: '8px',
          zIndex: 1
        }}>
          Loading 3D viewer...
        </div>
      )}
      <div style={{ position: 'relative' }}>
        <iframe
          src={EMBED_URL}
          style={{
            width: '100%',
            height: '500px',
            border: 'none',
            borderRadius: '8px',
            background: '#f5f5f5'
          }}
          onLoad={() => setLoading(false)}
          title="STL Viewer"
          allowFullScreen
        />
      </div>
      <div style={{ 
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginTop: '8px',
        fontSize: '12px',
        color: '#666'
      }}>
        <span>Drag to rotate • Scroll to zoom</span>
        <a 
          href={fileUrl} 
          target="_blank" 
          rel="noopener noreferrer"
          style={{ color: '#0066cc', textDecoration: 'none' }}
        >
          {fileName} ↗
        </a>
      </div>
    </div>
  );
};
