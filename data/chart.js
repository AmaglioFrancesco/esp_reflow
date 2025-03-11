var chartT = new Highcharts.Chart({
    chart:{
      renderTo:'temperatures_chart'
    },
    series: [
      {
        name: 'Ideal Temperature',
        type: 'line',
        color: '#FF7F50',
        marker: {
          symbol: 'circle',
          radius: 2,
          fillColor: '#FF7F50',
        }
      },
      {
        name: 'Real Temperature',
        type: 'line',
        color: '#6495ED',
        marker: {
          symbol: 'square',
          radius: 2,
          fillColor: '#6495ED',
        }
      },
    ],
    title: {
      text: undefined
    },
    xAxis: {
      type: 'datetime',
      dateTimeLabelFormats: { second: '%H:%M:%S' }
    },
    yAxis: {
      title: {
        text: 'Temperature (Celsius)'
      }
    }
  });
  
  
  //Plot ideal and real temperatures in the chart
  function plotTemperature(jsonValue) {
  
    var keys = Object.keys(jsonValue);
    console.log(keys);
    console.log(keys.length);
  
    for (var i = 0; i < keys.length; i++){
      var x = (new Date()).getTime();
      console.log(x);
      const key = keys[i];
      var y = Number(jsonValue[key]);
      console.log(y);
  
      if(chartT.series[i].data.length > 3000) {
        chartT.series[i].addPoint([x, y], true, true, true);
      } else {
        chartT.series[i].addPoint([x, y], true, false, true);
      }
  
    }
  }
  
  // Function to get current readings on the webpage when it loads for the first time
  function getReadings(){
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        var myObj = JSON.parse(this.responseText);
        console.log(myObj);
        plotTemperature(myObj);
      }
    };
    xhr.open("GET", "/readings", true);
    xhr.send();
  }
  
  if (!!window.EventSource) {
    var source = new EventSource('/events');
  
    source.addEventListener('open', function(e) {
      console.log("Events Connected");
    }, false);
  
    source.addEventListener('error', function(e) {
      if (e.target.readyState != EventSource.OPEN) {
        console.log("Events Disconnected");
      }
    }, false);
  
    source.addEventListener('message', function(e) {
      console.log("message", e.data);
    }, false);
  
    source.addEventListener('new_readings', function(e) {
      console.log("new_readings", e.data);
      var myObj = JSON.parse(e.data);
      console.log(myObj);
      plotTemperature(myObj);
    }, false);
  }