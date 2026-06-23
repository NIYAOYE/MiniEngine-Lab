import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath, URL } from 'node:url';

// Vite config — dev server + path alias `@` -> ./src.
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  server: {
    port: 5173,
    host: true,
    // Dev proxy: forward /api/* to the headless Tool server so the browser
    // never makes a cross-origin request (cpp-httplib sends no CORS headers).
    // toolClient calls `/api/invoke` and `/api/tools`; the `/api` prefix is
    // stripped before reaching http://127.0.0.1:8080.
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:8080',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api/, ''),
      },
    },
  },
});
