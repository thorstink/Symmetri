$(function begin() {
  const { mermaidAPI } = mermaid;

  conn = new WebSocket('ws://' + document.location.host + '/marking_transition_data');
  conn.onmessage = function (message) {
    console.warn('Here', mermaid);
    console.log(message.data)
    mermaidAPI.render('the-id', message.data, g => {
      document.querySelector('#mermaid-net').innerHTML = g;
    });
  }

});
