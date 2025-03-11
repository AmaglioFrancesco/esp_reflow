const temperature_min=20;
const temperature_max=300;
const time_min=10;
const time_max=1000;


let t1=document.querySelector(".t1");
let time=document.querySelector(".time");
let t2=document.querySelector(".t2");
let button=document.querySelector(".button");

button.disabled=true; //start with the button disabled

t1.addEventListener("change", stateHandle);
t2.addEventListener("change", stateHandle);
time.addEventListener("change", stateHandle);
button.addEventListener("click", buttonClicked);

function stateHandle(){
        console.log(parseInt(document.querySelector(".t1").value));
        console.log(parseInt(document.querySelector(".t2").value));
        console.log(parseInt(document.querySelector(".time").value));
        if(parseInt(document.querySelector(".t1").value)>temperature_min && parseInt(document.querySelector(".t1").value)<temperature_max 
            && parseInt(document.querySelector(".t2").value)>temperature_min && parseInt(document.querySelector(".t2").value)<temperature_max 
            && parseInt(document.querySelector(".time").value)>time_min && parseInt(document.querySelector(".time").value)<time_max){
                button.disabled= false;
        }
        else {
            button.disabled=true;
        }           
}

function buttonClicked(){
    document.getElementById("esit").innerHTML="Started Reflow!"
    document.getElementById("esit_t1").innerHTML="Preheat Temperature: "+ document.querySelector(".t1").value;
    document.getElementById("esit_time").innerHTML="Preheat Time: "+ document.querySelector(".time").value
    document.getElementById("esit_t2").innerHTML="Peak Temperature: "+ document.querySelector(".t2").value;
}