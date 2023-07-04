import { useEffect, useState } from "react";
import axios from 'axios';
import './styles.css';

function LocalizationResults() {
    const [lctype, setLctype] = useState("");
    const [methods, setMethods] = useState([]);
    var lcDescription = "";
    
    useEffect(() => {

        const fetchResults = async () => {
            axios.get('http://localhost:5001/cds/interResults')
            .then( (e) => {
                setLctype(e.data['lctype']);
                setMethods(e.data['methods']);
            })
            .catch( (e) => {
                console.error('Error: ', e)
                alert('Error: ' + e)
            })
        };

        fetchResults();
    }, []);

    if(lctype === "Type 0"){
        lcDescription = "Type 0 Contention means that the lock contention classifier found minimal or no lock contention within your Java program."
    }
    else if(lctype == "Type 1"){
        lcDescription = "Type 1 contention means that the lock contention classifier found that a thread(s) is holding the lock to a critical section for a prolonged time."
    }
    else{
        lcDescription = "Type 2 contention means that the lock contention classifier found that there is a high frequency of access requests from threads to acquire a particular lock."
    }
    //const methods = ["method1", "method2", "method3"]
    // const listMethods = methods.map(method =>
    //     <li>{method}</li>
    // );


    const getStacktraces = async () => {
        axios.get('http://localhost:5001/cds/getData', { params: { targetExtensions: 'stacktraces.log-rt' }, responseType: 'blob'})
            .then( (e) => {
                const href = URL.createObjectURL(e.data);
                const link = document.createElement('a');
                link.href = href;
                link.setAttribute('download', 'stacktraces.txt');
                document.body.appendChild(link)
                link.click();
                document.body.removeChild(link);
                URL.revokeObjectURL(href);
            })
            .catch( (e) => {
                console.error('Error: ', e)
                alert('Error: ' + e)
            })
    }

    
    return ( 
        <div className="container" style={{overflowY: 'scroll'}}>
            <h1 class="text-5xl font-bold">Results Until Fault Localization</h1>

            <div className="stats bg-primary text-primary-content" style={{margin: 50}}>
  
                <div className="stat">
                    <div className="stat-title">Lock Contention Type</div>
                    <div className="stat-value">{lctype}</div>
                    <div className="stat-actions">
                        <button onClick={ () => alert(lcDescription)} className="btn btn-sm">What does this mean?</button>
                    </div>
                </div>
                
                <div className="stat">
                    <div className="stat-title">Method(s)/Object(s) Causing Contention</div>
                    <ul style={{listStyleType: 'disc'}}>
                        {methods.map( method => <li>{method}</li> )}
                    </ul>
                    <div className="stat-actions">
                        <button onClick={e => getStacktraces()} style={{marginRight: 25}} className="btn btn-sm btn-success">View Complete Stack Traces</button>
                        <a href="/">
                            <button className="btn btn-sm">Return Home</button>
                        </a>
                    </div>
                </div>
            
            </div>

        </div>
    );
}

export default LocalizationResults;