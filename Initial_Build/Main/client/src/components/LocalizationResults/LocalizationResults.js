import { useEffect, useState } from "react";
import axios from 'axios';
import './styles.css';

const LocalizationResults = () => {
    return ( 
        <div className="container">
            <h1 class="text-5xl font-bold" style={{marginBottom: 50}}>Results Until Fault Localization</h1>


            <div className="stats bg-primary text-primary-content">
  
                <div className="stat">
                    <div className="stat-title">Account balance</div>
                    <div className="stat-value">$89,400</div>
                    <div className="stat-actions">
                    <button className="btn btn-sm btn-success">Add funds</button>
                    </div>
                </div>
                
                <div className="stat">
                    <div className="stat-title">Current balance</div>
                    <div className="stat-value">$89,400</div>
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