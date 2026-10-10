import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  serverExternalPackages: ["onnxruntime-web"],
  // Hostnames only (no scheme/port). Allow Cursor port-forward tunnels.
  allowedDevOrigins: [
    "127.0.0.1",
    "localhost",
    "*.cursor.sh",
    "*.cursorapi.com",
    "*.cursor.com",
    "*.cursorusercontent.com",
    "*.vscode-cdn.net",
    "vscode-webview",
    "*.trycloudflare.com",
  ],
  async headers() {
    return [
      {
        source: "/:path*",
        headers: [
          // Permite Web Serial no Chrome/Edge (ESP USB no PC local).
          {
            key: "Permissions-Policy",
            value: "serial=(self), camera=(self), microphone=()",
          },
        ],
      },
    ];
  },
};

export default nextConfig;
