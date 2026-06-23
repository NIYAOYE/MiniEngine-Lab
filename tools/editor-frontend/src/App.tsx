import { EngineProvider } from '@/state/engine';
import { AppShell } from '@/components/AppShell';

/** Root: provide engine state, then render the editor shell. */
export default function App() {
  return (
    <EngineProvider>
      <AppShell />
    </EngineProvider>
  );
}
