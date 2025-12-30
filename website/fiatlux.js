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
        // console.log("API response:", result);
    })
    .catch(error => {
        console.error("Error calling API:", error);
    });
}

function onLoad()
{
    // using github.com/jaames/iro.js
    var colorPicker = new iro.ColorPicker('#picker');

    // listen to a color picker's color:change event
    // color:change callbacks receive the current color
    colorPicker.on(['color:init', 'color:change'], function(color) {
        mycolor = color;
        mono();
        var btn = document.getElementById("monobutton");
        btn.style.backgroundColor = color.hexString;
    });
}

function mono()
{
    const url = `/led?red=${mycolor.red}&green=${mycolor.green}&blue=${mycolor.blue}`;
    trigger_restapi(url)
}

function rainbow()
{
    const url = `/rainbow?bright=${mycolor.value}`;
    trigger_restapi(url)
}

function speedSlide() 
{
    var slider = document.getElementById("speedRange");
    const url = `/speed?speed=${slider.value}`;
    trigger_restapi(url)
}
