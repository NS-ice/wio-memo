Import("env")

from pathlib import Path


def patch_rpcwifi_server():
    dependency_root = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
    candidates = list(dependency_root.glob("Seeed Arduino rpcWiFi*/src/WiFiServer.cpp"))
    if not candidates:
        return
    source = candidates[0]
    text = source.read_text(encoding="utf-8")
    marker = "fd_set readset;"
    if marker in text:
        return
    original = """WiFiClient WiFiServer::available(){
  if(!_listening)
    return WiFiClient();
  int client_sock;"""
    replacement = """WiFiClient WiFiServer::available(){
  if(!_listening)
    return WiFiClient();
  fd_set readset;
  FD_ZERO(&readset);
  FD_SET(sockfd, &readset);
  struct timeval timeout = {0, 0};
  if(lwip_select(sockfd + 1, &readset, nullptr, nullptr, &timeout) <= 0)
    return WiFiClient();
  int client_sock;"""
    if original not in text:
        print("rpcWiFi patch skipped: expected WiFiServer source was not found")
        return
    source.write_text(text.replace(original, replacement), encoding="utf-8")
    print("Applied rpcWiFi non-blocking accept patch")


patch_rpcwifi_server()
