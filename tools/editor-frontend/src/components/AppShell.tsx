import { useRef, useState } from 'react';
import { TopNavigation } from '@/components/TopNavigation';
import { StatusBar } from '@/components/StatusBar';
import { EntityTreePanel } from '@/components/EntityTreePanel';
import { EntityQuickActions } from '@/components/EntityQuickActions';
import { TimeSeasonPanel } from '@/components/TimeSeasonPanel';
import { FarmGrid } from '@/components/FarmGrid';
import { CropActionPanel } from '@/components/CropActionPanel';
import { InspectorPanel } from '@/components/InspectorPanel';
import { AuditLogPanel } from '@/components/AuditLogPanel';
import { ToolOverviewPanel } from '@/components/ToolOverviewPanel';
import { ToastHost } from '@/components/ui/Toast';

/**
 * Overall editor frame: top nav, scrollable three-column workspace + bottom
 * audit/tools region, fixed status bar. Below ~1100px the workspace keeps its
 * columns and scrolls horizontally (brief §十二) instead of stacking.
 */
export function AppShell() {
  const mainRef = useRef<HTMLDivElement>(null);
  const [activeTab, setActiveTab] = useState('section-scene');

  const navigate = (id: string) => {
    setActiveTab(id);
    const el = document.getElementById(id);
    el?.scrollIntoView({ behavior: 'smooth', block: 'start' });
  };

  return (
    <div className="flex flex-col h-screen bg-bg text-txt">
      <TopNavigation active={activeTab} onNavigate={navigate} />

      <main ref={mainRef} className="flex-1 min-h-0 overflow-auto">
        <div className="p-3 min-w-[1040px] flex flex-col gap-2.5">
          {/* three-column workspace */}
          <div className="flex gap-2.5 h-[600px]">
            {/* LEFT — entities */}
            <div id="section-scene" className="flex flex-col gap-2.5 w-[294px] shrink-0 min-h-0">
              <EntityTreePanel />
              <EntityQuickActions />
            </div>

            {/* CENTER — time + farm */}
            <div className="flex flex-col gap-2.5 flex-1 min-w-0 min-h-0">
              <div id="section-time">
                <TimeSeasonPanel />
              </div>
              <div id="section-crop" className="flex gap-2.5 flex-1 min-h-0">
                <FarmGrid />
                <div className="w-[300px] shrink-0 min-h-0 overflow-auto">
                  <CropActionPanel />
                </div>
              </div>
            </div>

            {/* RIGHT — inspector */}
            <InspectorPanel />
          </div>

          {/* bottom — audit + tools */}
          <div className="flex gap-2.5 h-[320px]">
            <div id="section-audit" className="flex-1 min-w-0 flex">
              <AuditLogPanel />
            </div>
            <div id="section-tools" className="flex">
              <ToolOverviewPanel />
            </div>
          </div>
        </div>
      </main>

      <StatusBar />
      <ToastHost />
    </div>
  );
}
