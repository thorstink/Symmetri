$(function begin() {
  const { mermaidAPI } = mermaid;

  $("#pause_button").click(function() {
    pause.send("Petri net paused");
    alert("Petri net paused");
    pause.send("Petri net unpaused");
  });

  pause = new WebSocket('ws://' + document.location.host + '/pause');
  conn = new WebSocket('ws://' + document.location.host + '/marking_transition_data');
  conn.onmessage = function (message) {
    console.warn('Here', mermaid);
    console.log(message.data)
    mermaidAPI.render('the-id', message.data, g => {
      document.querySelector('#mermaid-net').innerHTML = g;
    });
  }

});
