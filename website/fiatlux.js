
function onLoad()
{
    // using github.com/jaames/iro.js
    var colorPicker = new iro.ColorPicker('#picker');

    // listen to a color picker's color:change event
    // color:change callbacks receive the current color
    colorPicker.on('color:change', function(color) {
        // log the current color as a HEX string
        const url = `/led?red=${color.red}&green=${color.green}&blue=${color.blue}`;
        console.log(url);
        fetch(url, {
            method: "GET"
        })
        .then(response => {
            if (!response.ok) {
            throw new Error("Network response was not ok");
            }
            return response.json(); // or .text() if API returns text
        })
        .then(result => {
            console.log("API response:", result);
        })
        .catch(error => {
            console.error("Error calling API:", error);
        });
    });
}
