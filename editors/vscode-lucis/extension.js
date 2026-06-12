const { LanguageClient, TransportKind } = require("vscode-languageclient/node");
const vscode = require("vscode");

let client;

async function startClient(context) {
  const serverOptions = {
    command: "lucis",
    args: ["lsp"],
    transport: TransportKind.stdio,
  };

  const clientOptions = {
    documentSelector: [{ scheme: "file", language: "lucis" }],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher("**/*.lc"),
    },
  };

  client = new LanguageClient("lucis", "Lucis Language Server", serverOptions, clientOptions);
  await client.start();
}

function activate(context) {
  startClient(context);

  const restartCmd = vscode.commands.registerCommand("lucis.restartServer", async () => {
    if (client) {
      await client.stop();
      client = undefined;
    }
    await startClient(context);
    vscode.window.showInformationMessage("Lucis Language Server restarted.");
  });

  context.subscriptions.push(restartCmd);
  context.subscriptions.push({
    dispose: () => {
      if (client) {
        client.stop();
      }
    },
  });
}

function deactivate() {
  if (client) {
    return client.stop();
  }
}

module.exports = { activate, deactivate };
