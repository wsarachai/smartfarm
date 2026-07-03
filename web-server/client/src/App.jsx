import { Routes, Route, Navigate } from 'react-router-dom';
import AppShell from './components/AppShell';
import StatusHeader from './components/StatusHeader';
import Dashboard from './features/devices/Dashboard';
import CamerasPage from './features/camera/CamerasPage';
import IrrigationPage from './features/irrigation/IrrigationPage';
import AnalyticsPage from './features/analytics/AnalyticsPage';
import SettingsPage from './features/settings/SettingsPage';

export default function App() {
  return (
    <AppShell>
      <Routes>
        <Route
          path="/"
          element={
            <>
              <StatusHeader />
              <Dashboard />
            </>
          }
        />
        <Route path="/cameras" element={<CamerasPage />} />
        <Route path="/irrigation" element={<IrrigationPage />} />
        <Route path="/analytics" element={<AnalyticsPage />} />
        <Route path="/settings" element={<SettingsPage />} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </AppShell>
  );
}
