export default function PlayerModuleSection({ visible }: { visible: boolean }) {
  return (
    <section id="player-module" className={`doc-section${!visible ? " hidden-section" : ""}`}>
      <h1>Player Module</h1>
      <p><a href="#home">&#8592; Home</a></p>
      <hr />
      <p>Documentation coming soon.</p>
    </section>
  );
}
