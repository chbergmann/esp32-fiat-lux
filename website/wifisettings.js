
async function loadSettings()
{
    try {
        const response = await fetch("/values");

        if (!response.ok) {
            throw new Error("HTTP error " + response.status);
        }

        const data = await response.json();

        var ssid = document.getElementById("ssid");
        ssid.value = data.ssid;

        // Use them however you want (no page refresh)
    } catch (error) {
        console.error("Fetch error:", error);
    }
}
