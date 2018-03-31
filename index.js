function setup() {
    createCanvas(600, 450);
    background(0, 200, 0);

}

function draw() {

    ellipse(width/2, height/2, 50, 50);



}

function mouseClicked() {
    $.get("/settings", function(data, status){
        alert('OK');
        console.log(data);
    });
}