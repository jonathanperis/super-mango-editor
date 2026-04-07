export default function SourceFilesSection({ visible }: { visible: boolean }) {
  return (
    <section id="source-files" className={`doc-section${!visible ? " hidden-section" : ""}`}>
      <h1>Source Files</h1>
      <p><a href="#home">&#8592; Home</a></p>
      <hr />
      <p>Documentation coming soon.</p>
    </section>
  );
}
