# How to Start Ghidra with MCP

## Launch Ghidra
```
C:\tools\ghidra_12.0.3_PUBLIC\ghidraRun.bat
```

## First-Time Extension Install
1. In the Ghidra Project window: **File > Install Extensions > "+"**
2. Browse to: `C:\code\GIT\untitled_rev_eng_project\tools\ghidra_mcp\GhidraMCP-3.0.0.zip`
3. Restart Ghidra after installing

## Create / Open a Project
1. **File > New Project** (or File > Open Project)
2. Choose **Non-Shared Project**, click Next
3. Pick a directory and name it, click Finish

## Import a Binary
1. **File > Import File** and select a binary to analyze
2. Accept default import settings, click OK
3. Double-click the imported file to open it in **CodeBrowser**
4. When prompted to analyze, click **Yes** (use default analyzers)

## Start the MCP Server
1. In CodeBrowser: **File > Configure > Configure All Plugins** > check **GhidraMCP** > close
2. **Tools > GhidraMCP > Start MCP Server**
3. Verify with: `curl http://127.0.0.1:8089/health`

## After Starting
- Restart or start a new Claude Code session so the MCP tools connect
- The 179 ghidra-mcp tools will be available automatically via `.mcp.json`
