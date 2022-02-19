
// import another component
import mermaid from 'mermaid';
// import { mermaidAPI }   from 'mermaid';
import classes from '../css/main.css';

const { mermaidAPI } = mermaid;
console.warn('Here', mermaid);

mermaidAPI.render('the-id', `graph TD
A[Christmas] -->|Get money| B(Go shopping)
B --> C{Let me think}
C -->|One| D[Laptop]
C -->|Two| E[iPhone]
C -->|Three| F[fa:fa-car Car]`, g => {

  document.querySelector('#result').innerHTML(g);
});
