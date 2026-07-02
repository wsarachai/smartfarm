import Dashboard from './features/devices/Dashboard';
import CameraCard from './features/camera/CameraCard';

export default function App() {
  return (
    <div className="app">
      <header>
        <h1>Smart Farm Control Center</h1>
      </header>
      <section className="camera-section">
        <CameraCard />
      </section>
      <Dashboard />
    </div>
  );
}
