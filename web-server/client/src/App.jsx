import AppShell from './components/AppShell';
import StatusHeader from './components/StatusHeader';
import Dashboard from './features/devices/Dashboard';

export default function App() {
  return (
    <AppShell>
      <StatusHeader />
      <Dashboard />
    </AppShell>
  );
}
