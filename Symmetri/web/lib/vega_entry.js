$(function begin() {
    const spec = "http://localhost:2222/load_scheme.json";
    vegaEmbed('#vis', spec, {defaultStyle: true})
    .then(function(result) {
        const view = result.view;

        conn = new WebSocket('ws://' + document.location.host + '/transition_data');
        var window = 30 // sec
        // insert data as it arrives from the socket
        conn.onmessage = function(message) {
        data = vega.read(message.data, {type: 'csv', 'header': ['thread','start','end','task'] ,parse: {'thread': 'number', 'start': 'number', 'end':'number', 'task':'string'}});
        var changeSet = vega
            .changeset()
            .insert(data)
            .remove(function (t) {
                return t.end < (vega.peek(view.data('table')).end - window*1000)
            });
            view.change('table', changeSet).runTransition();
        }
        }).catch(console.warn);
});
