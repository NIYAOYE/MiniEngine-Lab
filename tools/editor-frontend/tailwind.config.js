/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        // Editor chrome — layered dark blue-greys (see brief §二).
        bg: '#0b121b',
        topbar: '#0e1722',
        p1: '#121c28',
        p2: '#182431',
        field: '#202c39',
        line: 'rgba(148,163,184,0.14)',
        // Text
        txt: '#e5edf5',
        sub: '#8f9cab',
        // Semantic accents
        accent: '#3b82f6',
        ok: '#69b84f',
        danger: '#d9544d',
        grape: '#7c4cc4',
        amber: '#c9892f',
      },
      borderRadius: {
        panel: '7px',
        ctl: '5px',
      },
      fontFamily: {
        sans: [
          'Inter',
          'PingFang SC',
          'Microsoft YaHei',
          'system-ui',
          'sans-serif',
        ],
        mono: ['"JetBrains Mono"', 'ui-monospace', 'SFMono-Regular', 'Menlo', 'monospace'],
      },
      fontSize: {
        '2xs': ['10.5px', '14px'],
      },
      boxShadow: {
        panel: '0 1px 0 rgba(255,255,255,0.02), 0 8px 24px -16px rgba(0,0,0,0.7)',
      },
    },
  },
  plugins: [],
};
