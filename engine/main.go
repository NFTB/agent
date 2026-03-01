package main

import (
	"bufio"
	"encoding/json"
	"log"
	"net"
	"os"
	"strings"
)

const defaultSockPath = "/tmp/agent-engine.sock"

func getSockPath() string {
	if p := os.Getenv("AGENT_SOCK"); p != "" {
		return p
	}
	return defaultSockPath
}

type Process struct {
	Pid  int    `json:"pid"`
	Name string `json:"name"`
}

type Heartbeat struct {
	Ts        int64     `json:"ts"`
	Processes []Process `json:"processes"`
}

// 可疑进程名规则（包含即告警）
var suspiciousNames = []string{
	"keylogger",
	"keylog",
	"malware",
	"miner",
	"cryptominer",
}

func isSuspicious(name string) bool {
	lower := strings.ToLower(name)
	for _, s := range suspiciousNames {
		if strings.Contains(lower, s) {
			return true
		}
	}
	return false
}

func handleConn(conn net.Conn) {
	defer conn.Close()
	scanner := bufio.NewScanner(conn)
	for scanner.Scan() {
		line := scanner.Bytes()
		if len(line) == 0 {
			continue
		}
		var hb Heartbeat
		if err := json.Unmarshal(line, &hb); err != nil {
			log.Printf("[engine] invalid JSON: %v", err)
			continue
		}
		// 日志：时间戳 + 进程数
		log.Printf("[heartbeat] ts=%d processes=%d", hb.Ts, len(hb.Processes))

		// 简单规则匹配
		for _, p := range hb.Processes {
			if isSuspicious(p.Name) {
				log.Printf("[ALERT] suspicious process: pid=%d name=%q", p.Pid, p.Name)
			}
		}
	}
	if err := scanner.Err(); err != nil {
		log.Printf("[engine] read error: %v", err)
	}
}

func main() {
	sockPath := getSockPath()
	if err := os.Remove(sockPath); err != nil && !os.IsNotExist(err) {
		log.Fatalf("remove socket: %v", err)
	}

	ln, err := net.Listen("unix", sockPath)
	if err != nil {
		log.Fatalf("listen: %v", err)
	}
	defer ln.Close()

	log.Printf("[engine] listening on %s", sockPath)

	for {
		conn, err := ln.Accept()
		if err != nil {
			log.Printf("accept: %v", err)
			continue
		}
		go (conn)
	}
}
