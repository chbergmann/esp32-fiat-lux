
async function loadSettings()
{
    try {
        const response = await fetch("/values");

        if (!response.ok) {
            throw new Error("HTTP error " + response.status);
        }

        const data = await response.json();

        var leds = document.getElementById("nr_leds");
        leds.value = data.nr_leds;
        var led1 = document.getElementById("led1");
        led1.value = data.led1;
        setRotation(data.rotate);

        // Use them however you want (no page refresh)
    } catch (error) {
        console.error("Fetch error:", error);
    }
}

function setRotation(dir)
{
    var r1 = document.getElementById("RadioButtonLeft");
    var r2 = document.getElementById("RadioButtonRight");
    var i1 = document.getElementById("imgleft");
    var i2 = document.getElementById("imgright");

    if(dir == "left") {
        r1.checked = true;
        r2.checked = false;
        i1.classList.add("active");
        i1.classList.remove("inactive");
        i2.classList.add("inactive");
        i2.classList.remove("active");
    }
    else {
        r1.checked = false;
        r2.checked = true;
        i1.classList.add("inactive");
        i1.classList.remove("active");
        i2.classList.add("active");
        i2.classList.remove("inactive");
    }
}