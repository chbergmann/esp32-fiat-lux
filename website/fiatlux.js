var mycolor;

function trigger_restapi(url)
{
    console.log(url);
    fetch(url, {
        method: "GET"
    })
    .then(response => {
        if (!response.ok) {
        throw new Error("Network response was not ok");
        }
        return response.text(); 
    })
    .then(result => {
        if(result == "RELOAD") {
            location.reload();
        }
        // console.log("API response:", result);
    })
    .catch(error => {
        console.error("Error calling API:", error);
    });
}

async function onLoad()
{
    var brightness = 100;
    try {
        const response1 = await fetch("/strips");
        if (!response1.ok) {
            throw new Error("HTTP error " + response1.status);
        }

        const data1 = await response1.json();
        if(data1.nr_strips > 1)
        {
            var stripselect = document.getElementById("stripselect");
            var html = "";
            for(var i=0; i<data1.nr_strips; i+=1) {
                html += `<button type='button' id='stripbtn${i}' class='imgbutton`;
                if(i == data1.selected_strip) {
                    html += " active";
                }
                html += `' onclick='stripselect(${i})'>${data1.name[i]}</button>`;
            }
            stripselect.innerHTML = html;
        }

        const response2 = await fetch(`/values?strip=${data1.selected_strip}`);

        if (!response2.ok) {
            throw new Error("HTTP error " + response.status);
        }

        const data = await response2.json();

        // Extract the 3 variables
        hexString = "#" + data.red.toString(16).padStart(2, '0') + data.green.toString(16).padStart(2, '0') + data.blue.toString(16).padStart(2, '0');
        brightness = data.bright;

        var slider = document.getElementById("speedRange");
        slider.value = data.speed;

        console.log("API response:", data);
        // Use them however you want (no page refresh)
    } catch (error) {
        console.error("Fetch error:", error);
    }

    var width = 320;
    if(window.innerWidth < window.innerHeight) {
        width = window.innerWidth * 0.75;
    }

    // using github.com/jaames/iro.js
    var colorPicker = new iro.ColorPicker('#picker', {
        // Set the size of the color picker
        width: width,
        // Set the initial color to pure red
        color: hexString,
        value: brightness
    });


    // listen to a color picker's color:change event
    // color:change callbacks receive the current color
    colorPicker.on(['color:init', 'color:change'], function(color) {
        mycolor = color;
        const url = `/led?red=${mycolor.red}&green=${mycolor.green}&blue=${mycolor.blue}&bright=${mycolor.value}`;
        trigger_restapi(url)
        var btn1 = document.getElementById("monobutton");
        btn1.style.backgroundColor = color.hexString;
        var btn2 = document.getElementById("gradientbutton");
        btn2.style.background = `linear-gradient(90deg,rgba(0, 0, 0, 1), rgba(${mycolor.red}, ${mycolor.green}, ${mycolor.blue}, 1), rgba(0, 0, 0, 1))`
    });
}

function mono()
{
    const url = `/mono?red=${mycolor.red}&green=${mycolor.green}&blue=${mycolor.blue}`;
    trigger_restapi(url);
}

function gradient()
{
    const url = `/gradient?red=${mycolor.red}&green=${mycolor.green}&blue=${mycolor.blue}`;
    trigger_restapi(url);
}

function rainbow()
{
    const url = `/rainbow?bright=${mycolor.value}`;
    trigger_restapi(url);
}

function rainbowclk()
{
    const url = `/rainbowclk?bright=${mycolor.value}`;
    trigger_restapi(url);
}

function walking()
{
    var slider = document.getElementById("speedRange");
    const url = `/walk?red=${mycolor.red}&green=${mycolor.green}&blue=${mycolor.blue}&speed=${slider.value}`;
    trigger_restapi(url);
}

function speedSlide() 
{
    var slider = document.getElementById("speedRange");
    const url = `/speed?speed=${slider.value}`;
    trigger_restapi(url);
}

function clock2()
{
    const url = `/clock2?bright=${mycolor.value}`;
    trigger_restapi(url);
}

function onoff()
{
    const url = `/power`;
    trigger_restapi(url);
}

function stripselect(nr)
{
    const url = `/led?strip=${nr}`;
    trigger_restapi(url);
}