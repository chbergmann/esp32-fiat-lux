
async function loadSettings()
{
    try {
        const response = await fetch("/getwifi");

        if (!response.ok) {
            throw new Error("HTTP error " + response.status);
        }

        const data = await response.json();

        var ssid = document.getElementById("ssid");
        ssid.value = data.ssid;
        var apname = document.getElementById("apname");
        apname.innerText = data.apname;
        var ap = document.getElementById("ap");
        ap.checked = data.use_ap == 1;

        // Use them however you want (no page refresh)
    } catch (error) {
        console.error("Fetch error:", error);
    }
}
