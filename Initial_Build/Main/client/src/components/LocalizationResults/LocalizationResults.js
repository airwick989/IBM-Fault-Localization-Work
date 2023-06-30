import { useEffect, useState } from "react";
import axios from 'axios';
import './styles.css';

function LocalizationResults() {
    
    // useEffect(() => {

    //     const fetchResults = async () => {
    //         axios.get('http://localhost:5001/cds/interResults')
    //             .then( (e) => {
    //                 console.log(e.data)
    //             })
    //             .catch( (e) => {
    //                 console.error('Error: ', e)
    //                 alert('Error: ' + e)
    //             })
    //     };

    //     fetchResults();
    // }, []);
    

    const methods = ["method1", "method2", "method3"]
    const listMethods = methods.map(method =>
        <li>{method}</li>
    );

    
    return ( 
        <div className="container">
            <h1 class="text-5xl font-bold">Results Until Fault Localization</h1>

            <div className="stats bg-primary text-primary-content" style={{margin: 50}}>
  
                <div className="stat">
                    <div className="stat-title">Lock Contention Type</div>
                    <div className="stat-value">$89,400</div>
                    <div className="stat-actions">
                    <button className="btn btn-sm btn-success">What does this mean?</button>
                    </div>
                </div>
                
                <div className="stat">
                    <div className="stat-title">Method(s) Causing Contention</div>
                    <ul>
                        {listMethods}
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