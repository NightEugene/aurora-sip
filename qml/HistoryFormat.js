.pragma library
function duration(seconds) {
    var n=Math.max(0, Math.floor(Number(seconds)))
    if (!isFinite(n)) return ""
    var h=Math.floor(n/3600)
    var m=Math.floor(n/60)%60
    var s=n%60
    return (h>0 ? h+":"+(m<10?"0":"") : "")+m+":"+(s<10?"0":"")+s
}
function details(row) {
    if (row.result === "Разговор" && row.durationSeconds !== undefined) return row.result+" · "+duration(row.durationSeconds)
    return row.result || ""
}
