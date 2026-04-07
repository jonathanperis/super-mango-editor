export default function SoundsSection({ visible }: { visible: boolean }) {
  return (
    <section id="sounds" className={`doc-section${!visible ? " hidden-section" : ""}`}>
      <h1>Sounds</h1>
      <p><a href="#home">&#8592; Home</a></p>
      <hr />
      <p>Documentation coming soon.</p>
    </section>
  );
}
