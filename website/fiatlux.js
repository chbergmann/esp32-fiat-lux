var bright = 100;

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
    colorPicker.on('color:change', function(color) {
        const url = `/led?red=${color.red}&green=${color.green}&blue=${color.blue}`;
        bright = color.value;
        trigger_restapi(url)
    });
}

function rainbow()
{
    const url = `/rainbow?bright=${bright}`;
    trigger_restapi(url)
}
