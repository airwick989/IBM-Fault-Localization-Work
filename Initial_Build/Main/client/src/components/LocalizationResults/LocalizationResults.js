import { useEffect, useState } from "react";
import axios from 'axios';
import './styles.css';

function LocalizationResults() {
    const [lctype, setLctype] = useState("");
    const [methods, setMethods] = useState([]);
    const [stacktraces, setStacktraces] = useState("");
    
    useEffect(() => {

        const fetchResults = async () => {
            axios.get('http://localhost:5001/cds/interResults')
                .then( (e) => {
                    setLctype(e.data['lctype']);
                    setMethods(e.data['methods']);
                    setStacktraces(e.data['stacktraces']);
                })
                .catch( (e) => {
                    console.error('Error: ', e)
                    alert('Error: ' + e)
                })
        };

        fetchResults();
    }, []);

    //const methods = ["method1", "method2", "method3"]
    // const listMethods = methods.map(method =>
    //     <li>{method}</li>
    // );

    
    return ( 
        <div className="container">
            <h1 class="text-5xl font-bold">Results Until Fault Localization</h1>

            <div className="stats bg-primary text-primary-content" style={{margin: 50}}>
  
                <div className="stat">
                    <div className="stat-title">Lock Contention Type</div>
                    <div className="stat-value">{lctype}</div>
                    <div className="stat-actions">
                    <button className="btn btn-sm btn-success">What does this mean?</button>
                    </div>
                </div>
                
                <div className="stat">
                    <div className="stat-title">Method(s)/Object(s) Causing Contention</div>
                    <ul style={{listStyleType: 'disc'}}>
                        {methods.map( method => <li>{method}</li> )}
                    </ul>
                    <div className="stat-actions">
                    <button className="btn btn-sm">Withdrawal</button> 
                    <button className="btn btn-sm">deposit</button>
                    </div>
                </div>
            
            </div>

        </div>
    );
}

export default LocalizationResults;