const fs = require("fs");

const source = fs.readFileSync("src/main.cpp", "utf8");
const htmlStart = source.indexOf('R"HTML(') + 7;
const htmlEnd = source.indexOf(')HTML"', htmlStart);
if (htmlStart < 7 || htmlEnd < 0) throw new Error("Embedded HTML was not found");

const html = source.slice(htmlStart, htmlEnd);
const scriptStart = html.indexOf("<script>") + 8;
const scriptEnd = html.indexOf("</script>", scriptStart);
if (scriptStart < 8 || scriptEnd < 0) throw new Error("Embedded script was not found");

new Function(html.slice(scriptStart, scriptEnd));
console.log(`Embedded HTML: ${html.length} bytes; JavaScript syntax: OK`);
