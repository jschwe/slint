// Copyright © SixtyFPS GmbH <info@slint-ui.com>
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Slint-commercial

import { createConnection, BrowserMessageReader, BrowserMessageWriter } from 'vscode-languageserver/browser';

import { ExecuteCommandParams, InitializeParams, InitializeResult } from 'vscode-languageserver';

import slint_init, * as slint_lsp from "../../../tools/lsp/pkg/index.js";
import slint_wasm_data from "../../../tools/lsp/pkg/index_bg.wasm";

slint_init(slint_wasm_data).then((_) => {

    const messageReader = new BrowserMessageReader(self);
    const messageWriter = new BrowserMessageWriter(self);
    const connection = createConnection(messageReader, messageWriter);

    let the_lsp: slint_lsp.SlintServer;

    function send_notification(method: string, params: any): boolean {
        connection.sendNotification(method, params);
        return true;
    }

    async function load_file(path: string): Promise<string> {
        const contents: Uint8Array = await connection.sendRequest("slint/load_file", path);
        return new TextDecoder().decode(contents);
    }

    connection.onInitialize((params: InitializeParams): InitializeResult => {
        the_lsp = slint_lsp.create(params, send_notification, load_file);
        return { capabilities: the_lsp.capabilities() };
    });

    connection.onRequest(async (method, params, token) => {
        if (method === "workspace/executeCommand" && (params as ExecuteCommandParams).command === "showPreview") {
            // forward back to the client so it can send the command to the webview
            return await connection.sendRequest("slint/showPreview", (params as ExecuteCommandParams).arguments);
        }
        return await the_lsp.handle_request(token, method, params);
    });

    connection.onDidChangeTextDocument(async (param) => {
        await the_lsp.reload_document(param.contentChanges[param.contentChanges.length - 1].text, param.textDocument.uri);
    });

    connection.onDidOpenTextDocument(async (param) => {
        await the_lsp.reload_document(param.textDocument.text, param.textDocument.uri);
    });

    // Listen on the connection
    connection.listen();

    // Now that we listen, the client is ready to send the init message
    self.postMessage("OK");
});




