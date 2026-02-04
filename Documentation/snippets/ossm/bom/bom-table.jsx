export const BOMTable = ({ items, columns = ["qty", "name"] }) => {
  if (!items || items.length === 0) return null;
  
  return (
    <table>
      <thead>
        <tr>
          {columns.includes("qty") && <th>Quantity</th>}
          {columns.includes("name") && <th>Part</th>}
          {columns.includes("length") && <th>Length</th>}
          {columns.includes("profile") && <th>Profile</th>}
        </tr>
      </thead>
      <tbody>
        {items.map((item, index) => (
          <tr key={index}>
            {columns.includes("qty") && <td>{item.qty}</td>}
            {columns.includes("name") && <td>{item.name}</td>}
            {columns.includes("length") && <td>{item.length}</td>}
            {columns.includes("profile") && <td>{item.profile}</td>}
          </tr>
        ))}
      </tbody>
    </table>
  );
};

export const ExtrusionTable = ({ items }) => {
  if (!items || items.length === 0) return null;
  
  return (
    <table>
      <thead>
        <tr>
          <th>Quantity</th>
          <th>Length</th>
          <th>Profile</th>
        </tr>
      </thead>
      <tbody>
        {items.map((item, index) => (
          <tr key={index}>
            <td>{item.qty}</td>
            <td>{item.length}</td>
            <td>{item.profile}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
};
